#include "eco_flow_erv.h"
#include "esphome/core/log.h"
#include <cstdio>

namespace esphome::eco_flow_erv {

static const char *const TAG = "eco_flow_erv";

static const char *mode_name(Mode mode) {
  switch (mode) {
    case Mode::SUPPLY: return "Supply";
    case Mode::EXHAUST: return "Exhaust";
    case Mode::RECOVERY: return "Recovery (experimental)";
  }
  return "Unknown";
}

void ControlSwitch::write_state(bool state) { this->parent_->enable_control(state); }
void ModeSelect::control(size_t index) { this->parent_->select_mode(index); }

void EcoFlowFan::setup() {
  // Reject timing overrides which could queue frames faster than the wire.
  const auto baud = this->parent_->get_baud_rate();
  if (baud < 550 || baud > 700 || this->frame_interval_ * baud < 48000) {
    ESP_LOGE(TAG, "Unsupported bench timing: use 550..700 baud and at least 48 bit cells per frame");
    this->mark_failed();
    return;
  }
  this->controller_.set_enabled(false);
  this->gate_->publish_state(false);
  this->mode_select_->publish_state(size_t(0));
  this->publish_requested_();
  if (this->active_ != nullptr) this->active_->publish_state(false);
  if (this->observed_ != nullptr) this->observed_->publish_state("No OUT data");
  ESP_LOGW(TAG, "Experimental IN candidate; control disabled. UART TX still drives its idle level");
}

void EcoFlowFan::dump_config() {
  LOG_FAN("", "Eco-Flo requested state", this);
  ESP_LOGCONFIG(TAG, "UART: %u baud, 8E2; see uart component config for pin polarity",
                unsigned(this->parent_->get_baud_rate()));
  ESP_LOGCONFIG(TAG, "Frame interval: %u ms; OUT timeout: %u ms", unsigned(this->frame_interval_),
                unsigned(this->out_timeout_));
  ESP_LOGCONFIG(TAG, "Diagnostics: every %u ms at DEBUG; per-frame TX/RX at VERBOSE; raw RX bytes at VERY_VERBOSE",
                unsigned(this->diagnostics_interval_));
  ESP_LOGCONFIG(TAG, "RX is observation only, not acknowledgement or measured airflow");
}

void EcoFlowFan::publish_requested_() {
  this->state = this->controller_.power();
  this->speed = this->controller_.speed();
  this->publish_state();
}

void EcoFlowFan::control(const fan::FanCall &call) {
  if (call.get_speed().has_value()) this->controller_.set_speed(*call.get_speed());
  if (call.get_state().has_value()) {
    if (*call.get_state() && !this->controller_.enabled())
      ESP_LOGW(TAG, "ON ignored: enable control first (arming starts with OFF frames)");
    this->controller_.set_power(*call.get_state());
  }
  this->publish_requested_();
  ESP_LOGI(TAG, "Request: power=%s speed=%u mode=%s control=%s (not confirmed by ERV)",
           ONOFF(this->controller_.power()), unsigned(this->controller_.speed()),
           mode_name(this->controller_.mode()), ONOFF(this->controller_.enabled()));
}

void EcoFlowFan::enable_control(bool enabled) {
  if (this->is_failed()) {
    this->gate_->publish_state(false);
    return;
  }
  const bool changed = enabled != this->controller_.enabled();
  this->controller_.set_enabled(enabled);
  if (changed) {
    // A deliberately muted interval is not scheduler lateness.
    this->has_tx_ = false;
    this->last_tx_gap_ = 0;
    this->max_tx_gap_ = 0;
  }
  this->gate_->publish_state(enabled);
  this->publish_requested_();
  if (changed) ESP_LOGW(TAG, "%s", enabled ? "Control armed: sending OFF until explicitly turned on"
                                                           : "Control muted; ERV may retain its last state");
}

void EcoFlowFan::select_mode(size_t index) {
  if (!this->mode_select_->has_index(index) || index > 2) return;
  this->controller_.set_mode(static_cast<Mode>(index));
  this->mode_select_->publish_state(index);
  ESP_LOGI(TAG, "Mode request: %s; power stays %s", mode_name(this->controller_.mode()),
           ONOFF(this->controller_.power()));
  // Neither selecting a direction nor selecting recovery changes power.
}

void EcoFlowFan::receive_(const Frame &frame, uint32_t now) {
  ++this->rx_frames_;
  if (!known_state(frame[1])) ++this->unknown_frames_;
  ESP_LOGV(TAG, "RX #%u t=%u ms: %02X %02X %02X (checksum valid, not an ACK)",
           unsigned(this->rx_frames_), unsigned(now), frame[0], frame[1], frame[2]);
  const bool changed = !this->out_live_ || frame[1] != this->last_rx_state_;
  this->last_rx_frame_ = now;
  this->last_rx_state_ = frame[1];
  this->out_live_ = true;
  if (this->active_ != nullptr) this->active_->publish_state(true);
  if (!changed) return;

  const auto state = frame[1];
  const char *meaning = "Unknown state";
  if (state == 0) meaning = "Off";
  else if (state >= 0x19 && state <= 0x1B) meaning = "Supply pattern";
  else if (state >= 0x09 && state <= 0x0B) meaning = "Exhaust / recovery A pattern";
  else if (state == 0x0D) meaning = "Recovery B pattern";
  else if (state == 0x0E || state == 0x0F) meaning = "Recovery B pattern (inferred)";
  char description[112];
  if (known_state(state) && state != 0)
    std::snprintf(description, sizeof(description), "%02X %02X %02X | %s | speed %u", frame[0], state,
                  frame[2], meaning, unsigned(state & 3));
  else
    std::snprintf(description, sizeof(description), "%02X %02X %02X | %s", frame[0], state, frame[2], meaning);
  ESP_LOGI(TAG, "OUT: %s", description);
  if (this->observed_ != nullptr) this->observed_->publish_state(description);
}

void EcoFlowFan::log_diagnostics_(uint32_t now) {
  if (uint32_t(now - this->last_diagnostics_) < this->diagnostics_interval_) return;
  this->last_diagnostics_ = now;
  const auto rejected = this->parser_.bad_checksum_candidates();
  ESP_LOGD(TAG, "Stats t=%u ms: control=%s TX=%u RX=%u RXbytes=%u bad_candidates=%u unknown=%u OUT=%s",
           unsigned(now), ONOFF(this->controller_.enabled()), unsigned(this->tx_frames_), unsigned(this->rx_frames_),
           unsigned(this->rx_bytes_), unsigned(rejected), unsigned(this->unknown_frames_),
           this->out_live_ ? "fresh" : "absent/stale");
  if (this->has_tx_)
    ESP_LOGD(TAG, "TX queued: last=%02X gap=%u ms max_gap_this_window=%u ms target=%u ms; wire timing unmeasured",
             this->last_tx_state_, unsigned(this->last_tx_gap_), unsigned(this->max_tx_gap_),
             unsigned(this->frame_interval_));
  if (this->rx_frames_ != 0)
    ESP_LOGD(TAG, "Last OUT=%02X age=%u ms (not measured airflow)", this->last_rx_state_,
             unsigned(uint32_t(now - this->last_rx_frame_)));
  if (rejected != this->last_bad_candidates_)
    ESP_LOGW(TAG, "Rejected %u checksum candidates this window; check polarity, baud and signal quality",
             unsigned(uint32_t(rejected - this->last_bad_candidates_)));
  this->last_bad_candidates_ = rejected;
  this->max_tx_gap_ = 0;
}

void EcoFlowFan::loop() {
  const uint32_t now = millis();
  // Byte budget prevents noise or a backlogged RX buffer from starving TX/API.
  for (unsigned n = 0; n < 64 && this->available(); ++n) {
    uint8_t byte;
    if (!this->read_byte(&byte)) break;
    ++this->rx_bytes_;
    ESP_LOGVV(TAG, "RX byte #%u t=%u ms: %02X", unsigned(this->rx_bytes_), unsigned(now), byte);
    if (uint32_t(now - this->last_rx_byte_) > 200) this->parser_.reset();
    this->last_rx_byte_ = now;
    Frame frame;
    if (this->parser_.push(byte, frame)) this->receive_(frame, now);
  }
  if (this->out_live_ && uint32_t(now - this->last_rx_frame_) >= this->out_timeout_) {
    ESP_LOGW(TAG, "OUT stale: no checksum-valid frame for %u ms; TX policy unchanged",
             unsigned(uint32_t(now - this->last_rx_frame_)));
    this->out_live_ = false;
    this->parser_.reset();
    if (this->active_ != nullptr) this->active_->publish_state(false);
    if (this->observed_ != nullptr) this->observed_->publish_state("OUT stale / disconnected");
  }
  Frame frame;
  if (this->controller_.next_frame(now, frame)) {
    this->write_array(frame);
    ++this->tx_frames_;
    if (this->has_tx_) {
      this->last_tx_gap_ = uint32_t(now - this->last_tx_time_);
      if (this->last_tx_gap_ > this->max_tx_gap_) this->max_tx_gap_ = this->last_tx_gap_;
    }
    if (!this->has_tx_ || frame[1] != this->last_tx_state_)
      ESP_LOGI(TAG, "TX state #%u t=%u ms: %02X %02X %02X; queued, acceptance unknown",
               unsigned(this->tx_frames_), unsigned(now), frame[0], frame[1], frame[2]);
    this->has_tx_ = true;
    this->last_tx_state_ = frame[1];
    this->last_tx_time_ = now;
    ESP_LOGV(TAG, "TX #%u t=%u ms: %02X %02X %02X gap=%u ms", unsigned(this->tx_frames_), unsigned(now),
             frame[0], frame[1], frame[2], unsigned(this->last_tx_gap_));
  }
  this->log_diagnostics_(now);
}

}  // namespace esphome::eco_flow_erv

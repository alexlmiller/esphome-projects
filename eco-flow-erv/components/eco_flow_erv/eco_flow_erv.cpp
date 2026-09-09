#include "eco_flow_erv.h"
#include "esphome/core/log.h"
#include <cstdio>

namespace esphome::eco_flow_erv {

static const char *const TAG = "eco_flow_erv";

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
  ESP_LOGCONFIG(TAG, "Frame interval: %u ms; OUT timeout: %u ms", unsigned(this->frame_interval_),
                unsigned(this->out_timeout_));
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
}

void EcoFlowFan::enable_control(bool enabled) {
  if (this->is_failed()) {
    this->gate_->publish_state(false);
    return;
  }
  const bool changed = enabled != this->controller_.enabled();
  this->controller_.set_enabled(enabled);
  this->gate_->publish_state(enabled);
  this->publish_requested_();
  if (changed) ESP_LOGW(TAG, "%s", enabled ? "Control armed: sending OFF until explicitly turned on"
                                                           : "Control muted; ERV may retain its last state");
}

void EcoFlowFan::select_mode(size_t index) {
  if (!this->mode_select_->has_index(index) || index > 2) return;
  this->controller_.set_mode(static_cast<Mode>(index));
  this->mode_select_->publish_state(index);
  // Neither selecting a direction nor selecting recovery changes power.
}

void EcoFlowFan::receive_(const Frame &frame, uint32_t now) {
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

void EcoFlowFan::loop() {
  const uint32_t now = millis();
  // Byte budget prevents noise or a backlogged RX buffer from starving TX/API.
  for (unsigned n = 0; n < 64 && this->available(); ++n) {
    uint8_t byte;
    if (!this->read_byte(&byte)) break;
    if (uint32_t(now - this->last_rx_byte_) > 200) this->parser_.reset();
    this->last_rx_byte_ = now;
    Frame frame;
    if (this->parser_.push(byte, frame)) this->receive_(frame, now);
  }
  if (this->out_live_ && uint32_t(now - this->last_rx_frame_) >= this->out_timeout_) {
    this->out_live_ = false;
    this->parser_.reset();
    if (this->active_ != nullptr) this->active_->publish_state(false);
    if (this->observed_ != nullptr) this->observed_->publish_state("OUT stale / disconnected");
  }
  Frame frame;
  if (this->controller_.next_frame(now, frame)) {
    this->write_array(frame);
    ESP_LOGV(TAG, "TX candidate: %02X %02X %02X", frame[0], frame[1], frame[2]);
  }
}

}  // namespace esphome::eco_flow_erv

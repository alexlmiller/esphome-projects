#pragma once

#include <array>
#include <cstdint>

namespace esphome::eco_flow_erv {

using Frame = std::array<uint8_t, 3>;
enum class Mode : uint8_t { SUPPLY, EXHAUST, RECOVERY };

// OUT-derived candidates, not yet proven commands accepted by IN.
inline Frame encode(bool power, uint8_t speed, Mode mode, bool phase_b = false) {
  uint8_t state = 0;
  if (power && speed >= 1 && speed <= 3) {
    switch (mode) {
      case Mode::SUPPLY: state = 0x18 | speed; break;
      case Mode::EXHAUST: state = 0x08 | speed; break;
      case Mode::RECOVERY: state = (phase_b ? 0x0C : 0x08) | speed; break;
    }
  }
  return {0x05, state, static_cast<uint8_t>(0x05 + state)};
}

inline bool known_state(uint8_t state) {
  return state == 0 || (state >= 0x09 && state <= 0x0B) ||
         (state >= 0x0D && state <= 0x0F) || (state >= 0x19 && state <= 0x1B);
}

// No allocation, bounded sliding window. Accept structurally valid unknown
// states for diagnostics too; never convert RX into a control command.
class Parser {
 public:
  bool push(uint8_t byte, Frame &frame) {
    this->window_[0] = this->window_[1];
    this->window_[1] = this->window_[2];
    this->window_[2] = byte;
    if (this->length_ < 3) ++this->length_;
    if (this->length_ == 3 && this->window_[0] == 0x05) {
      if (this->window_[2] != static_cast<uint8_t>(0x05 + this->window_[1])) {
        ++this->bad_checksum_candidates_;
        return false;
      }
      frame = this->window_;
      this->length_ = 0;
      return true;
    }
    return false;
  }
  void reset() { this->length_ = 0; this->window_ = {}; }
  // Counts rejected 05-prefixed windows, NOT an exact dropped-packet count.
  // Stream resynchronization resets the window but retains this lifetime count.
  uint32_t bad_checksum_candidates() const { return this->bad_checksum_candidates_; }

 protected:
  Frame window_{};
  uint8_t length_{0};
  uint32_t bad_checksum_candidates_{0};
};

// The component and host tests use the same scheduler/state machine. No catch-
// up bursts after a stalled loop. Unsigned subtraction handles millis() wrap.
class Controller {
 public:
  bool enabled() const { return this->enabled_; }
  bool power() const { return this->power_; }
  uint8_t speed() const { return this->speed_; }
  Mode mode() const { return this->mode_; }
  bool phase_b() const { return this->phase_b_; }
  void set_interval(uint32_t ms) { this->interval_ms_ = ms; }
  void set_phase_packets(uint16_t packets) { this->phase_packets_ = packets; }

  void set_enabled(bool enabled) {
    if (enabled == this->enabled_) return;
    this->enabled_ = enabled;
    this->power_ = false;  // Arming never starts a previously requested speed.
    // Preserve the last TX time across rapid disarm/re-arm so an in-flight
    // packet cannot be followed immediately by another queued packet.
    this->reset_phase_();
  }
  void set_power(bool power) {
    const bool next = power && this->enabled_;
    if (next != this->power_) this->reset_phase_();
    this->power_ = next;
  }
  void set_speed(uint8_t speed) {
    if (speed >= 1 && speed <= 3) this->speed_ = speed;
  }
  void set_mode(Mode mode) {
    if (mode != this->mode_) this->reset_phase_();
    this->mode_ = mode;
  }
  // Relative deadline for the component's one-shot TX timer. Check enabled()
  // before scheduling; retain this deadline across a quick disarm/re-arm.
  uint32_t next_frame_delay(uint32_t now) const {
    if (!this->sent_) return 0;
    const uint32_t elapsed = now - this->last_tx_;
    return elapsed < this->interval_ms_ ? this->interval_ms_ - elapsed : 0;
  }
  bool next_frame(uint32_t now, Frame &frame) {
    if (!this->enabled_ || this->next_frame_delay(now) != 0)
      return false;
    this->last_tx_ = now;
    this->sent_ = true;
    frame = encode(this->power_, this->speed_, this->mode_, this->phase_b_);
    if (this->power_ && this->mode_ == Mode::RECOVERY && ++this->phase_count_ >= this->phase_packets_) {
      this->phase_count_ = 0;
      this->phase_b_ = !this->phase_b_;
    }
    return true;
  }

 protected:
  void reset_phase_() { this->phase_b_ = false; this->phase_count_ = 0; }
  bool enabled_{false};
  bool power_{false};
  bool sent_{false};
  bool phase_b_{false};
  uint8_t speed_{1};
  Mode mode_{Mode::SUPPLY};
  uint16_t phase_count_{0};
  uint16_t phase_packets_{700};
  uint32_t interval_ms_{97};
  uint32_t last_tx_{0};
};

}  // namespace esphome::eco_flow_erv

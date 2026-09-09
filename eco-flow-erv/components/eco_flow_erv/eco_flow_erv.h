#pragma once

#include "protocol.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::eco_flow_erv {

class EcoFlowFan;

class ControlSwitch : public switch_::Switch {
 public:
  explicit ControlSwitch(EcoFlowFan *parent) : parent_(parent) {}
 protected:
  void write_state(bool state) override;
  EcoFlowFan *parent_;
};

class ModeSelect : public select::Select {
 public:
  explicit ModeSelect(EcoFlowFan *parent) : parent_(parent) {}
 protected:
  void control(size_t index) override;
  EcoFlowFan *parent_;
};

class EcoFlowFan : public Component, public fan::Fan, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  fan::FanTraits get_traits() override { return fan::FanTraits(false, true, false, 3); }
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_frame_interval(uint32_t ms) { this->frame_interval_ = ms; this->controller_.set_interval(ms); }
  void set_phase_packets(uint16_t n) { this->controller_.set_phase_packets(n); }
  void set_out_timeout(uint32_t ms) { this->out_timeout_ = ms; }
  void set_control_switch(ControlSwitch *gate) { this->gate_ = gate; }
  void set_mode_select(ModeSelect *mode) { this->mode_select_ = mode; }
  void set_observed_frame(text_sensor::TextSensor *sensor) { this->observed_ = sensor; }
  void set_out_active(binary_sensor::BinarySensor *sensor) { this->active_ = sensor; }
  void enable_control(bool enabled);
  void select_mode(size_t index);

 protected:
  void control(const fan::FanCall &call) override;
  void publish_requested_();
  void receive_(const Frame &frame, uint32_t now);
  Controller controller_;
  Parser parser_;
  ControlSwitch *gate_{nullptr};
  ModeSelect *mode_select_{nullptr};
  text_sensor::TextSensor *observed_{nullptr};
  binary_sensor::BinarySensor *active_{nullptr};
  uint32_t frame_interval_{97};
  uint32_t out_timeout_{2000};
  uint32_t last_rx_byte_{0};
  uint32_t last_rx_frame_{0};
  uint8_t last_rx_state_{0};
  bool out_live_{false};
};

}  // namespace esphome::eco_flow_erv

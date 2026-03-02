#pragma once

#include "AnalogWritePin.h"
#include "DigitalWritePin.h"
#include "StoveThrottle.h"
#include <cstdint>

class StoveActuator {
public:
  StoveActuator(AnalogWritePin &pwm_pin, DigitalWritePin &bypass_pin, const ThrottleConfig &config);
  virtual ~StoveActuator() = default;

  virtual void setThrottle(StoveThrottle throttle);
  virtual void update();

private:
  AnalogWritePin &pwm_pin_;
  DigitalWritePin &bypass_pin_;
  const ThrottleConfig config_;

  uint32_t current_boost_ = 0;
  bool is_boost_pulse_active_ = false;
  uint32_t last_boost_change_ms_ = 0;
  StoveThrottle throttle_ = {};
  StoveThrottle printed_throttle_ = {};
};

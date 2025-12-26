#pragma once

#include "Potentiometer.h"
#include "DigitalWritePin.h"
#include "StoveThrottle.h"
#include <cstdint>

class StoveActuator {
public:
  StoveActuator(Potentiometer &potentiometer, DigitalWritePin &bypass_pin, const ThrottleConfig &config);
  virtual ~StoveActuator() = default;

  virtual void setBypass();
  virtual void setThrottle(const StoveThrottle &throttle);
  virtual void update();

private:
  Potentiometer &potentiometer_;
  DigitalWritePin &bypass_pin_;
  const ThrottleConfig config_;

  bool is_bypass_;
  uint32_t current_boost_;
  bool is_boost_pulse_active_;
  uint32_t last_boost_change_ms_ = 0;
  StoveThrottle printed_throttle_ = {};
  StoveThrottle target_throttle_ = {};
};

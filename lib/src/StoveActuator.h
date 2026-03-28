#pragma once

#include "AnalogWritePin.h"
#include "DigitalWritePin.h"
#include "StoveThrottle.h"
#include <cstdint>
#include <limits>

class StoveActuator {
public:
  StoveActuator(AnalogWritePin &pwm_pin, const ThrottleConfig &config);
  virtual ~StoveActuator() = default;

  virtual void setThrottle(StoveThrottle throttle);
  virtual void setMinThrottle();
  virtual void update();

private:
  void updateTargetPwm(uint32_t now_ms);
  void writeSlewedPwm(uint32_t now_ms);

  AnalogWritePin &pwm_pin_;
  const ThrottleConfig config_;

  uint32_t current_boost_ = 0;
  bool is_boost_pulse_active_ = false;
  uint32_t last_boost_change_ms_ = 0;
  StoveThrottle throttle_ = {};
  StoveThrottle printed_throttle_ = {};

  float target_pwm_ = 0.0f;
  float current_pwm_ = 0.0f;
  uint32_t last_update_ms_ = std::numeric_limits<int32_t>::max();
};

#pragma once

#include "StoveActuator.h"
#include "StoveThrottle.h"
#include <cstdint>
#include <limits>

class StoveController {
public:
  StoveController(StoveActuator &actuator, const ThrottleConfig &config);
  virtual ~StoveController() = default;

  virtual void setThrottle(StoveThrottle throttle);
  virtual void update();

private:
  void updateTargetPwm(uint32_t now_ms);

  StoveActuator &actuator_;
  const ThrottleConfig config_;

  uint32_t current_boost_ = 0;
  bool is_boost_pulse_active_ = false;
  uint32_t last_boost_change_ms_ = 0;
  StoveThrottle throttle_ = {};
  StoveThrottle printed_throttle_ = {};

  float target_pwm_ = 0.0f;
};

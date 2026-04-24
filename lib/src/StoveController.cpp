#include "StoveController.h"
#include "Logger.h"
#include "sfloat.h"
#include <algorithm>

extern "C" uint32_t millis();

StoveController::StoveController(StoveActuator &actuator,
                             const ThrottleConfig &config)
    : actuator_(actuator), config_(config) {}

void StoveController::setThrottle(StoveThrottle throttle) {
  if (!isNear(throttle, printed_throttle_)) {
    Log << "StoveController::setThrottle(/*position=*/" << throttle.position
        << ", /*boost=*/" << throttle.boost << ")\n";
    printed_throttle_ = throttle;
  }
  throttle_ = throttle;
}

void StoveController::update() {
  uint32_t now_ms = millis();
  updateTargetPwm(now_ms);
  actuator_.write(target_pwm_);
}

void StoveController::updateTargetPwm(uint32_t now_ms) {
  float value = std::max(config_.min, throttle_.position * config_.max);

  if (throttle_.boost == current_boost_) {
    target_pwm_ = value;
    return;
  }

  if (throttle_.boost < current_boost_) {
    target_pwm_ = std::min(value, config_.max - 0.1f);
    current_boost_ = 0;
    is_boost_pulse_active_ = false;
    last_boost_change_ms_ = now_ms;
    return;
  }

  if (now_ms - last_boost_change_ms_ < 1000) {
    target_pwm_ = is_boost_pulse_active_ ? config_.boost : config_.max;
    return;
  }

  if (is_boost_pulse_active_) {
    target_pwm_ = config_.max;
    ++current_boost_;
  } else {
    target_pwm_ = config_.boost;
  }

  is_boost_pulse_active_ = !is_boost_pulse_active_;
  last_boost_change_ms_ = now_ms;
}


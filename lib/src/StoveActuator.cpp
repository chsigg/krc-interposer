#include "StoveActuator.h"
#include "DigitalWritePin.h"
#include "Logger.h"
#include "sfloat.h"
#include <algorithm>

extern "C" uint32_t millis();

StoveActuator::StoveActuator(AnalogWritePin &pwm_pin,
                             const ThrottleConfig &config)
    : pwm_pin_(pwm_pin), config_(config) {}

void StoveActuator::setThrottle(StoveThrottle throttle) {
  if (!isNear(throttle, printed_throttle_)) {
    Log << "StoveActuator::setThrottle(/*position=*/" << throttle.position
        << ", /*boost=*/" << throttle.boost << ")\n";
    printed_throttle_ = throttle;
  }
  throttle_ = throttle;
}

void StoveActuator::setMinThrottle() { setThrottle({config_.min, 0}); }

void StoveActuator::update() {
  uint32_t now_ms = millis();
  updateTargetPwm(now_ms);
  writeSlewedPwm(now_ms);
}

void StoveActuator::updateTargetPwm(uint32_t now_ms) {
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

void StoveActuator::writeSlewedPwm(uint32_t now_ms) {
  float max_delta = (now_ms - last_update_ms_) * 0.001f;
  current_pwm_ = std::clamp(target_pwm_, current_pwm_ - max_delta,
                            current_pwm_ + max_delta);
  pwm_pin_.write(current_pwm_);
  last_update_ms_ = now_ms;
}

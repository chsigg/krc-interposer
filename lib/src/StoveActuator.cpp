#include "StoveActuator.h"
#include "DigitalWritePin.h"
#include "Logger.h"
#include "sfloat.h"
#include <algorithm>

extern "C" uint32_t millis();

StoveActuator::StoveActuator(AnalogWritePin &pwm_pin,
                             const ThrottleConfig &config)
    : pwm_pin_(pwm_pin), config_(config) {
}

void StoveActuator::setThrottle(StoveThrottle throttle) {
  if (!isNear(throttle, printed_throttle_)) {
    Log << "StoveActuator::setThrottle(/*position=*/" << throttle.position
        << ", /*boost=*/" << throttle.boost << ")\n";
    printed_throttle_ = throttle;
  }
  throttle_ = throttle;

  update();
}

void StoveActuator::update() {
  float value = throttle_.position * config_.max;

  if (throttle_.boost == current_boost_) {
    pwm_pin_.write(value);
    return;
  }

  uint32_t now_ms = millis();
  if (throttle_.boost < current_boost_) {
    float deboost_value = std::min(value, config_.max - 0.1f);
    pwm_pin_.write(deboost_value);
    current_boost_ = 0;
    is_boost_pulse_active_ = false;
    last_boost_change_ms_ = now_ms;
    return;
  }

  if (now_ms - last_boost_change_ms_ < 1000) {
    return;
  }

  if (is_boost_pulse_active_) {
    pwm_pin_.write(config_.max);
    ++current_boost_;
  } else {
    pwm_pin_.write(config_.boost);
  }

  is_boost_pulse_active_ = !is_boost_pulse_active_;
  last_boost_change_ms_ = now_ms;
}

#include "StoveActuator.h"
#include "DigitalWritePin.h"
#include "Logger.h"
#include "sfloat.h"
#include <algorithm>

extern "C" uint32_t millis();

StoveActuator::StoveActuator(Potentiometer &potentiometer,
                             DigitalWritePin &bypass_pin,
                             const ThrottleConfig &config)
    : potentiometer_(potentiometer), bypass_pin_(bypass_pin), config_(config) {}

void StoveActuator::setBypass() {
  if (is_bypass_) {
    return;
  }
  Log << "StoveActuator::setBypass()\n";
  bypass_pin_.set(PinState::Low);
  current_boost_ = config_.num_boosts;
  is_bypass_ = true;
  is_boost_pulse_active_ = false;
}

void StoveActuator::setThrottle(StoveThrottle throttle) {
  if (is_bypass_ || !isNear(throttle, printed_throttle_)) {
    Log << "StoveActuator::setThrottle(/*position=*/" << throttle.position
        << ", /*boost=*/" << throttle.boost << ")\n";
    printed_throttle_ = throttle;
  }
  throttle_ = throttle;

  if (is_bypass_) {
    bypass_pin_.set(PinState::High);
    is_bypass_ = false;
  }

  update();
}

void StoveActuator::update() {
  if (is_bypass_) {
    return;
  }

  float value = throttle_.position * config_.max;

  if (throttle_.boost == current_boost_) {
    potentiometer_.setValue(value);
    return;
  }

  uint32_t now = millis();
  if (throttle_.boost < current_boost_) {
    float deboost_value = std::min(value, config_.max - 0.1f);
    potentiometer_.setValue(deboost_value);
    current_boost_ = 0;
    is_boost_pulse_active_ = false;
    last_boost_change_ms_ = now;
    return;
  }

  if (now - last_boost_change_ms_ < 1000) {
    return;
  }

  if (is_boost_pulse_active_) {
    potentiometer_.setValue(config_.max);
    ++current_boost_;
  } else {
    potentiometer_.setValue(config_.boost);
  }

  is_boost_pulse_active_ = !is_boost_pulse_active_;
  last_boost_change_ms_ = now;
}

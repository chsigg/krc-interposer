#include "StoveActuator.h"
#include "DigitalWritePin.h"
#include "Logger.h"
#include "sfloat.h"
#include <algorithm>

extern "C" uint32_t millis();

StoveActuator::StoveActuator(Potentiometer &potentiometer,
                             DigitalWritePin &bypass_pin,
                             const ThrottleConfig &config)
    : potentiometer_(potentiometer), bypass_pin_(bypass_pin), config_(config), is_bypass_(false) {
}

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

void StoveActuator::setThrottle(const StoveThrottle &throttle) {
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

  const float delta = (config_.arm - config_.max) / 2;
  const float deboost_value = config_.max - delta;
  const float arm_value = config_.max + delta;

  float value = std::min(throttle_.position * config_.max, arm_value);

  if (throttle_.boost == current_boost_) {
    potentiometer_.setValue(value);
    return;
  }

  uint32_t now = millis();
  if (throttle_.boost < current_boost_) {
    potentiometer_.setValue(std::min(deboost_value, value));
    current_boost_ = 0;
    is_boost_pulse_active_ = false;
    last_boost_change_ms_ = now;
    return;
  }

  if (now - last_boost_change_ms_ < 1000) {
    return;
  }

  if (is_boost_pulse_active_) {
    potentiometer_.setValue(arm_value);
    ++current_boost_;
  } else {
    potentiometer_.setValue(1.0f);
  }

  is_boost_pulse_active_ = !is_boost_pulse_active_;
  last_boost_change_ms_ = now;
}

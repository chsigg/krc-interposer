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
    Log << "StoveActuator::setThrottle(/*base=*/" << throttle.base
        << ", /*boost=*/" << throttle.boost << ")\n";
    printed_throttle_ = throttle;
  }
  target_throttle_ = throttle;

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

  const float delta = (config_.boost - config_.max) / 2;
  const float below_max_level = config_.max - delta;
  const float above_max_level = config_.max + delta;

  float position = std::min(target_throttle_.base * config_.max, above_max_level);

  if (target_throttle_.boost == current_boost_) {
    potentiometer_.setPosition(position);
    return;
  }

  uint32_t now = millis();
  if (target_throttle_.boost < current_boost_) {
    potentiometer_.setPosition(std::min(below_max_level, position));
    current_boost_ = 0;
    is_boost_pulse_active_ = false;
    last_boost_change_ms_ = now;
    return;
  }

  if (now - last_boost_change_ms_ < 1000) {
    return;
  }

  if (is_boost_pulse_active_) {
    potentiometer_.setPosition(above_max_level);
    ++current_boost_;
  } else {
    potentiometer_.setPosition(1.0f);
  }

  is_boost_pulse_active_ = !is_boost_pulse_active_;
  last_boost_change_ms_ = now;
}

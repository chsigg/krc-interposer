#include "StoveActuator.h"
#include "Logger.h"
#include "sfloat.h"
#include <algorithm>

extern "C" uint32_t millis();

StoveActuator::StoveActuator(DigiPot &pot, const ThrottleConfig &config)
    : pot_(pot), config_(config) {}

void StoveActuator::setPosition(float position) {
  pot_.setPosition(position);
  is_direct_mode_ = true;
}

void StoveActuator::setThrottle(const StoveThrottle &throttle) {
  if (is_direct_mode_ || !isNear(throttle, printed_throttle_)) {
    Log << "StoveActuator::setThrottle(/*base=*/" << throttle.base
        << ", /*boost=*/" << throttle.boost << ")\n";
    printed_throttle_ = throttle;
  }

  float delta = (config_.boost - config_.max) / 2;
  float below_max_level = config_.max - delta;
  float above_max_level = config_.max + delta;

  float position = throttle.base * config_.max;
  if (throttle.base >= 1.0f) {
    position = above_max_level;
  }

  uint32_t now = millis();
  if (is_direct_mode_ || throttle.boost < current_boost_) {
    pot_.setPosition(std::min(below_max_level, position));
    current_boost_ = 0;
    last_boost_change_ms_ = now;
    is_direct_mode_ = false;
  }

  if (now - last_boost_change_ms_ < 1000) {
    return;
  }

  if (throttle.boost == current_boost_) {
    pot_.setPosition(position);
    return;
  }

  if (pot_.getPosition() <= config_.boost) {
    pot_.setPosition(1.0f);
    ++current_boost_;
  } else {
    pot_.setPosition(above_max_level);
  }

  last_boost_change_ms_ = now;
}

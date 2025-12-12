#include "StoveActuator.h"
#include "Logger.h"
#include "sfloat.h"
#include <algorithm>

StoveActuator::StoveActuator(DigiPot &pot, const ThrottleConfig &config)
    : pot_(pot), config_(config) {}

void StoveActuator::setThrottle(const StoveThrottle &throttle) {
  if (std::abs(throttle.base - target_throttle_.base) > 0.05f ||
      throttle.boost != target_throttle_.boost) {
    Log << "StoveActuator::setThrottle(/*base=*/" << throttle.base
        << ", /*boost=*/" << throttle.boost << ")\n";
  }
  target_throttle_ = throttle;
}

void StoveActuator::update() {
  if (target_throttle_.boost < current_boost_) {
    float below_max_level = config_.max - (config_.boost - config_.max) / 2;
    pot_.setPosition(std::min(below_max_level, target_throttle_.base));
    current_boost_ = 0;
    return;
  }

  if (target_throttle_.boost == current_boost_) {
    pot_.setPosition(target_throttle_.base);
    return;
  }

  if (pot_.getPosition() <= config_.boost) {
    pot_.setPosition(1.0f);
    ++current_boost_;
  } else {
    float above_max_level = config_.max + (config_.boost - config_.max) / 2;
    pot_.setPosition(above_max_level);
  }
}

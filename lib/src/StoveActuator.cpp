#include "StoveActuator.h"
#include "Logger.h"
#include "sfloat.h"
#include <algorithm>

extern "C" uint32_t millis();

StoveActuator::StoveActuator(DigiPot &pot, const ThrottleConfig &config)
    : pot_(pot), config_(config) {}

void StoveActuator::setThrottle(const StoveThrottle &throttle) {
  if (!isNear(throttle, printed_throttle_)) {
    Log << "StoveActuator::setThrottle(/*base=*/" << throttle.base
        << ", /*boost=*/" << throttle.boost << ")\n";
    printed_throttle_ = throttle;
  }
  target_throttle_ = throttle;
}

void StoveActuator::update() {
  uint32_t now = millis();
  if (target_throttle_.boost < current_boost_) {
    float below_max_level = config_.max - (config_.boost - config_.max) / 2;
    pot_.setPosition(std::min(below_max_level, target_throttle_.base));
    current_boost_ = 0;
    last_boost_change_ms_ = now;
  }

  if (now - last_boost_change_ms_ < 1000) {
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

  last_boost_change_ms_ = now;
}

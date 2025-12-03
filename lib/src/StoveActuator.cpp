#include "StoveActuator.h"
#include <algorithm>

StoveActuator::StoveActuator(DigiPot &pot, const LevelConfig &config)
    : pot_(pot), config_(config) {}

void StoveActuator::setLevel(const StoveLevel &level) { target_level_ = level; }

void StoveActuator::update() {
  if (target_level_.boost < current_boost_) {
    float below_max_level = config_.max - (config_.boost - config_.max) / 2;
    pot_.setLevel(std::min(below_max_level, target_level_.base));
    current_boost_ = 0;
    return;
  }

  if (target_level_.boost == current_boost_) {
    pot_.setLevel(target_level_.base);
    return;
  }

  if (pot_.getLevel() <= config_.boost) {
    pot_.setLevel(1.0f);
    ++current_boost_;
  } else {
    float above_max_level = config_.max + (config_.boost - config_.max) / 2;
    pot_.setLevel(above_max_level);
  }
}

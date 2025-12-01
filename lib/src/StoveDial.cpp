#include "StoveDial.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

StoveDial::StoveDial(AnalogReadPin &pin, const StoveDialConfig &config)
    : pin_(pin), config_(config) {
  assert(config_.min < config_.max);
  assert(config_.max < config_.boost);
}

void StoveDial::update() {
  auto begin = last_readings_.begin();
  auto end = last_readings_.end();

  std::rotate(begin, begin+1, end);
  last_readings_.back() = pin_.read();

  int sum = std::accumulate(begin, end, 0);
  int reading = sum / last_readings_.size();

  current_level_ = std::min(static_cast<float>(reading) / config_.max, 1.0f);

  if (reading < config_.min) {
    current_level_ = 0.0f;
  }

  if (reading < config_.boost) {
    boost_armed_ = true;
  }

  if (reading < config_.max) {
    current_boost_ = 0;
    return;
  }

  if (reading < config_.boost || !boost_armed_ ||
      current_boost_ >= config_.num_boosts) {
    return;
  }

  current_boost_++;
  boost_armed_ = false;
}

float StoveDial::getLevel() const { return current_level_; }

int StoveDial::getBoostLevel() const { return current_boost_; }

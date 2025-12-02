#include "StoveDial.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

StoveDial::StoveDial(const AnalogReadPin &pin, const StoveLevelConfig &config)
    : pin_(pin), config_(config) {
  assert(config_.min < config_.max);
  assert(config_.max < config_.boost);
}

void StoveDial::update() {
  auto begin = last_readings_.begin();
  auto end = last_readings_.end();

  std::move(begin + 1, end, begin);
  last_readings_.back() = pin_.read();

  int32_t sum = std::accumulate(begin, end, int32_t(0));
  int32_t reading = sum / last_readings_.size();

  level_.base = std::min(static_cast<float>(reading) / config_.max, 1.0f);

  if (reading < config_.min) {
    level_.base = 0.0f;
  }

  if (reading < config_.max) {
    level_.boost = 0;
    return;
  }

  if (reading < config_.boost) {
    boost_armed_ = true;
    return;
  }

  if (!boost_armed_ || level_.boost >= config_.num_boosts) {
    return;
  }

  ++level_.boost;
  boost_armed_ = false;
}

StoveLevel StoveDial::getLevel() const { return level_; }

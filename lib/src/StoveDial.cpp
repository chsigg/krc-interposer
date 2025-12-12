#include "StoveDial.h"
#include "StoveThrottle.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

StoveDial::StoveDial(const AnalogReadPin &pin, const ThrottleConfig &config)
    : pin_(pin), config_(config) {
  assert(config_.min < config_.max);
  assert(config_.max < config_.boost);
}

void StoveDial::update() {
  auto begin = last_readings_.begin();
  auto end = last_readings_.end();

  std::move(begin + 1, end, begin);
  last_readings_.back() = pin_.read();

  float sum = std::accumulate(begin, end, 0.0f);
  float reading = sum / last_readings_.size();

  throttle_.base = std::min(reading / config_.max, 1.0f);

  if (reading < config_.min) {
    throttle_.base = 0.0f;
  }

  if (reading < config_.max) {
    throttle_.boost = 0;
    return;
  }

  if (reading < config_.boost) {
      boost_armed_ = true;
    return;
  }

  if (!boost_armed_ || throttle_.boost >= config_.num_boosts) {
    return;
  }

  ++throttle_.boost;
  boost_armed_ = false;
}

StoveThrottle StoveDial::getThrottle() const { return throttle_; }

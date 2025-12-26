#include "StoveDial.h"
#include "Logger.h"
#include "StoveThrottle.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
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
  position_ = std::clamp(sum / last_readings_.size(), 0.0f, 1.0f);

  throttle_.base = std::min(position_ / config_.max, 1.0f);

  if (position_ < config_.min) {
    throttle_.base = 0.0f;
  }

  std::unique_ptr<StoveDial, void (*)(StoveDial *)> printer(
      this, [](StoveDial *dial) {
        if (std::fabs(dial->position_ - dial->printed_position_) < 0.02f) {
          return;
        }
        Log << "StoveDial::update() position " << dial->position_
            << ", throttle " << dial->throttle_.base << ", boost "
            << dial->throttle_.boost << "\n";
        dial->printed_position_ = dial->position_;
      });

  if (position_ < config_.max) {
    throttle_.boost = 0;
    return;
  }

  if (position_ < config_.arm) {
    boost_armed_ = true;
    return;
  }

  if (!boost_armed_ || position_ < config_.boost ||
      throttle_.boost >= config_.num_boosts) {
    return;
  }

  ++throttle_.boost;
  boost_armed_ = false;
}

StoveThrottle StoveDial::getThrottle() const { return throttle_; }

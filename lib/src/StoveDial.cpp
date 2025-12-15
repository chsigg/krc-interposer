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

static void printThrottle(const StoveThrottle &throttle) {
}

void StoveDial::update() {
  auto begin = last_readings_.begin();
  auto end = last_readings_.end();

  std::move(begin + 1, end, begin);
  last_readings_.back() = pin_.read();

  float sum = std::accumulate(begin, end, 0.0f);
  float reading = sum / last_readings_.size();
  reading /= 0.85f;  // Voltage divider

  throttle_.base = std::min(reading / config_.max, 1.0f);

  if (reading < config_.min) {
    throttle_.base = 0.0f;
  }

  std::unique_ptr<StoveDial, void (*)(StoveDial *)> printer(
      this, [](StoveDial *dial) {
        if (isNear(dial->throttle_, dial->printed_throttle_)) {
          return;
        }
        Log << "StoveDial::update() throttle " << dial->throttle_.base
            << ", boost " << dial->throttle_.boost << "\n";
        dial->printed_throttle_ = dial->throttle_;
      });

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

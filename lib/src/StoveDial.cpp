#include "StoveDial.h"
#include "Logger.h"
#include "StoveThrottle.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <numeric>

StoveDial::StoveDial(const AnalogReadPin &pin, const ThrottleConfig &config)
    : pin_(pin), config_(config) {}

void StoveDial::update() {
  auto begin = last_readings_.begin();
  auto end = last_readings_.end();

  std::move(begin + 1, end, begin);
  last_readings_.back() = pin_.read();

  float sum = std::accumulate(begin, end, 0.0f);
  value_ = std::clamp(sum / last_readings_.size(), 0.0f, 1.0f);

  if (value_ <= config_.max) {
    position_ = std::max(0.0f, value_ / config_.max);
  }

  if (std::fabs(value_ - printed_value_) < 0.02f) {
    return;
  }
  Log << "StoveDial::update() value " << value_ << ", position " << position_
      << "\n";
  printed_value_ = value_;
}

float StoveDial::getPosition() const { return position_; }

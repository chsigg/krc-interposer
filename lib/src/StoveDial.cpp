#include "StoveDial.h"
#include "Logger.h"
#include "StoveThrottle.h"
#include <algorithm>
#include <cmath>

StoveDial::StoveDial(const DialSensor &sensor, const ThrottleConfig &config)
    : sensor_(sensor), config_(config) {}

void StoveDial::update() {
  value_ = sensor_.read();

  if (value_ <= config_.max) {
    position_ = std::max(0.0f, value_ / config_.max);
  }

  if (std::fabs(value_ - printed_value_) < 0.05f) {
    return;
  }
  Log << "StoveDial::update() value " << value_ << "\n";
  printed_value_ = value_;
}

float StoveDial::getPosition() const { return position_; }

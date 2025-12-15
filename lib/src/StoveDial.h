#pragma once

#include "AnalogReadPin.h"
#include "StoveThrottle.h"
#include <array>
#include <cstdint>

class StoveDial {
public:
  StoveDial(const AnalogReadPin &pin, const ThrottleConfig &config);

  float getPosition() const { return position_; }

  void update();
  virtual StoveThrottle getThrottle() const;

private:
  const AnalogReadPin &pin_;
  const ThrottleConfig config_;

  StoveThrottle throttle_ = {};
  StoveThrottle printed_throttle_ = {};

  std::array<float, 4> last_readings_ = {};
  float position_ = 0.0f;
  bool boost_armed_ = true;
};

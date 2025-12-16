#pragma once

#include "AnalogReadPin.h"
#include "StoveThrottle.h"
#include <array>
#include <cstdint>

class StoveDial {
public:
  StoveDial(const AnalogReadPin &pin, const ThrottleConfig &config);
  virtual ~StoveDial() = default;

  virtual float getPosition() const { return position_; }
  virtual bool isOff() const { return position_ < config_.min; }
  virtual StoveThrottle getThrottle() const;

  virtual void update();

private:
  const AnalogReadPin &pin_;
  const ThrottleConfig config_;

  StoveThrottle throttle_ = {};
  StoveThrottle printed_throttle_ = {};

  std::array<float, 4> last_readings_ = {};
  float position_ = 0.0f;
  bool boost_armed_ = true;
};

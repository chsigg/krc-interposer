#pragma once

#include "AnalogReadPin.h"
#include "StoveThrottle.h"
#include <array>
#include <cstdint>

class StoveDial {
public:
  StoveDial(const AnalogReadPin &pin, const ThrottleConfig &config);
  virtual ~StoveDial() = default;

  virtual float getPosition() const;
  virtual bool isOff() const { return value_ < config_.min; }
  virtual bool isBoil() const { return value_ > config_.boil; }
  virtual void update();

private:
  const AnalogReadPin &pin_;
  const ThrottleConfig config_;

  std::array<float, 4> last_readings_ = {};
  float value_ = 0.0f;
  float printed_value_ = 0.0f;
};

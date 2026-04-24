#pragma once

#include "DialSensor.h"
#include "StoveThrottle.h"
#include <array>
#include <cstdint>

class StoveDial {
public:
  StoveDial(const DialSensor &sensor, const ThrottleConfig &config);
  virtual ~StoveDial() = default;

  virtual float getPosition() const;
  virtual bool isOff() const { return value_ < config_.off; }
  virtual bool isBoil() const { return value_ > config_.boil; }
  virtual void update();

private:
  const DialSensor &sensor_;
  const ThrottleConfig config_;

  float value_ = 0.0f;
  float printed_value_ = 0.0f;
  float position_ = 0.0f;
};

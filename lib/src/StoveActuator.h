#pragma once

#include "DigiPot.h"
#include "StoveThrottle.h"
#include <cstdint>

class StoveActuator {
public:
  StoveActuator(DigiPot &pot, const ThrottleConfig &config);
  virtual ~StoveActuator() = default;

  virtual void setPosition(float position);
  virtual void setThrottle(const StoveThrottle &throttle);

private:
  DigiPot &pot_;
  const ThrottleConfig config_;

  bool is_direct_mode_ = true;
  uint32_t current_boost_ = 0;
  uint32_t last_boost_change_ms_ = 0;
  StoveThrottle printed_throttle_ = {};
};

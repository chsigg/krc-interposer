#pragma once

#include <cstdint>

// Maps analog pin reading to stove throttle
struct ThrottleConfig {
  float min = 0.1f;   // level min/max, level 0.0 below
  float max = 0.8f;   // boost 0 below, level 1.0 at and above
  float boost = 0.9f; // increments boost above
  uint32_t num_boosts = 2;
};

struct StoveThrottle {
  float base;
  uint32_t boost;
};

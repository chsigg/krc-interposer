#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>

// Maps analog pin reading to stove throttle
struct ThrottleConfig {
  float min = 0.05f;    // level min, level 0.0 below
  float max = 0.7f;    // boost 0 below, level 1.0 at and above
  float arm = 0.78f;   // arms boost below
  float boost = 0.82f; // increments boost above
  float boil = 0.9f;   // auto mode above
  uint32_t num_boosts = 2;
};

struct StoveThrottle {
  float position;
  uint32_t boost; // if boost > 0, position must be 1.0
};

inline bool isNear(const StoveThrottle &a, const StoveThrottle &b) {
  return std::fabs(a.position - b.position) <= 0.05f && a.boost == b.boost;
}

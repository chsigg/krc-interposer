#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>

// Maps analog pin reading to stove throttle
struct ThrottleConfig { // measured values in parenthesis
  float off = 0.025f;   // threshold to be considered off
  float min = 0.15f;    // (0.050) safe minimum pwm output
  float max = 0.8f;     // (0.794) level 1.0, boost 0 below
  float boost = 0.85f;  // (0.846) increments boost
  float boil = 0.9f;    // (0.940) boil mode above
  uint32_t num_boosts = 2;
};

struct StoveThrottle {
  float position;
  uint32_t boost; // if boost > 0, position must be 1.0
};

inline bool isNear(const StoveThrottle &a, const StoveThrottle &b) {
  return std::fabs(a.position - b.position) <= 0.05f && a.boost == b.boost;
}

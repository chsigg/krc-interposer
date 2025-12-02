#pragma once

#include <cstdint>

// Maps analog pin reading to stove level
struct StoveLevelConfig {
  int32_t min;   // level min/max, level 0.0 below
  int32_t max;   // boost 0 below, level 1.0 at and above
  int32_t boost; // increments boost above
  int32_t num_boosts;
};

struct StoveLevel {
  float base;
  int32_t boost;
};

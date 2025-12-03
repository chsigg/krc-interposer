#pragma once

#include <cstdint>

// Maps analog pin reading to stove level
struct StoveLevelConfig {
  float min = 0.1f;   // level min/max, level 0.0 below
  float max = 0.8f;   // boost 0 below, level 1.0 at and above
  float boost = 0.9f; // increments boost above
  int32_t num_boosts = 2;
};

struct StoveLevel {
  float base;
  int32_t boost;
};

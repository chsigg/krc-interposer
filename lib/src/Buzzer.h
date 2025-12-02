#pragma once

#include <cstdint>

class Buzzer {
public:
  virtual ~Buzzer() = default;

  virtual void enable(int32_t frequency_hz) = 0;

  virtual void disable() = 0;
};

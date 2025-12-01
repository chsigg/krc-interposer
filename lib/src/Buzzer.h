#pragma once

#include <cstdint>

class Buzzer {
public:
  virtual ~Buzzer() = default;

  virtual void enable() = 0;

  virtual void disable() = 0;
};

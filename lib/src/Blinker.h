#pragma once

#include "DigitalWritePin.h"
#include <cstdint>

class Blinker {
public:
  enum class Signal {
    NONE,
    ONCE,
    REPEAT,
  };

  explicit Blinker(DigitalWritePin &led);

  void blink(Signal signal);
  void update();

private:
  DigitalWritePin &led_;

  uint32_t step_ = 0;
  uint32_t next_step_time_ms_ = 0;
};

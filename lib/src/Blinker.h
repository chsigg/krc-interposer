#pragma once

#include "DigitalWritePin.h"
#include <cstdint>
#include <sys/types.h>

class Blinker {
public:
  enum class Signal : uint8_t {
    NONE,
    ONCE,
    REPEAT,
  };

  explicit Blinker(DigitalWritePin &led);

  void blink(Signal signal);
  void update();

private:
  DigitalWritePin &led_;

  uint8_t step_ = 0;
  uint32_t next_step_time_ms_ = 0;
};

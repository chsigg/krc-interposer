#pragma once

#include "Led.h"
#include <cstdint>
#include <sys/types.h>

class Blinker final {
public:
  enum class Signal : uint8_t {
    NONE,
    ONCE,
    REPEAT,
    SOLID,
  };

  explicit Blinker(Led &led);

  void blink(Signal signal);
  void update();

private:
  Led &led_;

  uint8_t step_ = kIdleStep;
  uint32_t step_end_ms_ = 0;

  static constexpr uint8_t kIdleStep = 255;
};

#pragma once

#include "Buzzer.h"
#include <cstdint>
#include <sys/types.h>

class Beeper {
public:
  enum class Signal : uint8_t {
    NONE,
    ACCEPT,
    REJECT,
    ERROR,
  };

  explicit Beeper(Buzzer &buzzer);
  virtual ~Beeper() = default;

  virtual void beep(Signal signal);

  virtual void update();

private:
  Buzzer &buzzer_;

  uint8_t step_ = 0;
  uint32_t step_end_ms_ = 0;
};

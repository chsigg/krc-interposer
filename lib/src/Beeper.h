#pragma once

#include "Buzzer.h"
#include <cstdint>
#include <sys/types.h>

class Beeper {
public:
  enum class Signal : uint8_t {
    NONE,
    POWER_ON,
    POWER_OFF,
    CONNECTED,
    DISCONNECTED,
  };

  explicit Beeper(Buzzer &buzzer);
  virtual ~Beeper() = default;

  virtual void beep(Signal signal);
  virtual void update();
  virtual bool isIdle() const;

private:
  Buzzer &buzzer_;
  uint8_t step_ = kIdleStep;
  uint32_t step_end_ms_ = 0;

  static constexpr uint8_t kIdleStep = 255;
};

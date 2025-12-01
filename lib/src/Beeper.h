#pragma once

#include "Buzzer.h"
#include <cstdint>

class Beeper {
public:
  explicit Beeper(Buzzer &buzzer);

  void beep(int32_t duration_ms);

  void update();

private:
  Buzzer &buzzer_;

  uint32_t end_time_ms_ = 0;
};

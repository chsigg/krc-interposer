#pragma once

#include "Buzzer.h"

class Beeper {
public:
  explicit Beeper(Buzzer &buzzer);

  void beep(int duration_ms);

  void update();

private:
  Buzzer &buzzer_;

  unsigned long end_time_ms_ = 0;
};

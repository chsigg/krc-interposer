#include "Beeper.h"
#include <algorithm>
#include <cstdint>

extern "C" uint32_t millis();

Beeper::Beeper(Buzzer &buzzer) : buzzer_(buzzer) {}

void Beeper::beep(uint32_t duration_ms) {
  if (duration_ms <= 0) {
    return;
  }
  end_time_ms_ = std::max(end_time_ms_, millis() + duration_ms);
  buzzer_.enable(4000);
}

void Beeper::update() {
  if (end_time_ms_ <= 0 || millis() < end_time_ms_) {
    return;
  }

  buzzer_.disable();
  end_time_ms_ = 0;
}

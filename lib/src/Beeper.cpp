#include "Beeper.h"
#include <algorithm>
#include <cstdint>
#include <iterator>

extern "C" uint32_t millis();

Beeper::Beeper(Buzzer &buzzer) : buzzer_(buzzer) {}

void Beeper::beep(Signal signal) {
  step_ = static_cast<uint32_t>(signal);
  next_step_time_ms_ = millis();
}

void Beeper::update() {

  static constexpr uint16_t LOW_FREQ = 2000;
  static constexpr uint16_t HIGH_FREQ = 4000;
  static constexpr uint16_t TONE_DURATION_MS = 100;
  static constexpr uint16_t SILENT_DURATION_MS = 1000;

  static constexpr struct {
      uint16_t duration_ms;
      uint16_t frequency_hz;
      uint8_t next_step;
  } STATES[] = {
      {0, 0, 9},
      {TONE_DURATION_MS,LOW_FREQ, 4},
      {TONE_DURATION_MS, HIGH_FREQ, 5},
      {TONE_DURATION_MS,LOW_FREQ, 6},
      {TONE_DURATION_MS,HIGH_FREQ, 0},
      {TONE_DURATION_MS,LOW_FREQ, 0},
      {TONE_DURATION_MS,0, 7},
      {TONE_DURATION_MS,LOW_FREQ, 8},
      {SILENT_DURATION_MS,0, 3},
  };

  if (step_ >= std::size(STATES) || millis() < next_step_time_ms_) {
    return;
  }

  auto state = STATES[step_];

  if (state.frequency_hz > 0) {
    buzzer_.enable(state.frequency_hz);
  } else {
    buzzer_.disable();
  }

  step_ = state.next_step;
  next_step_time_ms_ = millis() + state.duration_ms;
}

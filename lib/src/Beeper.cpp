#include "Beeper.h"
#include <algorithm>
#include <cstdint>

extern "C" uint32_t millis();

Beeper::Beeper(Buzzer &buzzer) : buzzer_(buzzer) {}

void Beeper::beep(Signal signal) {
  static constexpr uint32_t FIRST_STEPS[] = {0, 1, 4, 7};
  step_ = FIRST_STEPS[static_cast<uint32_t>(signal)];
  next_step_time_ms_ = millis();
}

void Beeper::update() {
  if (millis() < next_step_time_ms_) {
    return;
  }

  static constexpr uint16_t LOW_FREQ = 2000;
  static constexpr uint16_t HIGH_FREQ = 4000;
  static constexpr uint16_t TONE_DURATION_MS = 100;
  static constexpr uint16_t SILENT_DURATION_MS = 1000;

  static constexpr struct {
    union {
      uint16_t frequency;
      uint8_t step;
    };
    uint16_t duration;
  } BEEPER_STEPS[] = {
      // None
      {{.step = 0}, 0},
      // Accept
      {LOW_FREQ, TONE_DURATION_MS},
      {HIGH_FREQ, TONE_DURATION_MS},
      {{.step = 0}, 0},
      // Reject
      {HIGH_FREQ, TONE_DURATION_MS},
      {LOW_FREQ, TONE_DURATION_MS},
      {{.step = 0}, 0},
      // Error
      {LOW_FREQ, TONE_DURATION_MS},
      {0, TONE_DURATION_MS},
      {LOW_FREQ, TONE_DURATION_MS},
      {0, SILENT_DURATION_MS},
      {{.step = 7}, 0},
  };

  auto step = BEEPER_STEPS[step_];

  if (step.frequency > 0) {
    buzzer_.enable(step.frequency);
  } else {
    buzzer_.disable();
  }
  if (step_ == 0) {
    return;
  }
  next_step_time_ms_ = millis() + step.duration;

  ++step_;
  step = BEEPER_STEPS[step_];
  if (step.duration == 0) {
    step_ = step.step;
  }
}

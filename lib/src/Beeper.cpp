#include "Beeper.h"
#include "Logger.h"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>

extern "C" uint32_t millis();

Beeper::Beeper(Buzzer &buzzer) : buzzer_(buzzer) {}

void Beeper::beep(Signal signal) {
  Log << "Beeper::beep(" << static_cast<uint32_t>(signal) << ")\n";
  step_ = static_cast<uint32_t>(signal);
  step_end_ms_ = millis();
  update();
}

void Beeper::update() {

  static constexpr uint16_t LOW_FREQ = 800;
  static constexpr uint16_t HIGH_FREQ = 1200;
  static constexpr uint16_t TONE_DURATION_MS = 100;
  static constexpr uint16_t SILENT_DURATION_MS = 1000;

  static constexpr struct {
    uint16_t duration_ms;
    uint16_t frequency_hz;
    uint8_t next_step;
  } STATES[] = {
      {0, 0, 255},
      {TONE_DURATION_MS, LOW_FREQ, 4},
      {TONE_DURATION_MS, HIGH_FREQ, 5},
      {TONE_DURATION_MS, LOW_FREQ, 6},
      {TONE_DURATION_MS, HIGH_FREQ, 0},
      {TONE_DURATION_MS, LOW_FREQ, 0},
      {TONE_DURATION_MS, 0, 7},
      {TONE_DURATION_MS, LOW_FREQ, 8},
      {SILENT_DURATION_MS, 0, 3},
  };

  uint32_t now = millis();
  while (step_ < std::size(STATES)) {

    if (now - step_end_ms_ > std::numeric_limits<int32_t>::max()) {
      return;
    }

    auto state = STATES[step_];

    if (state.frequency_hz > 0) {
      buzzer_.enable(state.frequency_hz);
    } else {
      buzzer_.disable();
    }

    step_ = state.next_step;
    step_end_ms_ = now + state.duration_ms;
  }
}

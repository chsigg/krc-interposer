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
  step_ = static_cast<uint8_t>(signal);
  step_end_ms_ = millis();
  update();
}

void Beeper::update() {

  static constexpr uint16_t kFreq800Hz = 800;
  static constexpr uint16_t kFreq1200Hz = 1200;
  static constexpr uint16_t kFreq1600Hz = 1600;
  static constexpr uint16_t kToneDurationMs = 200;
  static constexpr uint16_t kSilentDurationMs = 1000;

  static constexpr struct {
    uint16_t duration_ms;
    uint16_t frequency_hz;
    uint8_t next_step;
  } STATES[] = {
      {0, 0, kIdleStep},
      {kToneDurationMs, kFreq800Hz, 5},
      {kToneDurationMs, kFreq1200Hz, 6},
      {kToneDurationMs, kFreq1200Hz, 7},
      {kToneDurationMs, kFreq800Hz, 8},
      {kToneDurationMs, kFreq1200Hz, 0},
      {kToneDurationMs, kFreq800Hz, 0},
      {kToneDurationMs, kFreq1600Hz, 0},
      {kToneDurationMs, 0, 9},
      {kToneDurationMs, kFreq800Hz, 10},
      {kSilentDurationMs, 0, 4},
  };

  uint32_t now_ms = millis();
  while (step_ < std::size(STATES)) {

    if (now_ms - step_end_ms_ > std::numeric_limits<int32_t>::max()) {
      return;
    }

    auto state = STATES[step_];

    if (state.frequency_hz > 0) {
      buzzer_.enable(state.frequency_hz);
    } else {
      buzzer_.disable();
    }

    step_ = state.next_step;
    step_end_ms_ = now_ms + state.duration_ms;
  }
}

bool Beeper::isIdle() const {
  return step_ == kIdleStep;
}

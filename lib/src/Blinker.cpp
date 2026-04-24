#include "Blinker.h"
#include "Led.h"
#include "Logger.h"
#include <cstdint>
#include <iterator>
#include <limits>

extern "C" uint32_t millis();

Blinker::Blinker(Led &led) : led_(led) {}

void Blinker::blink(Signal signal) {
  Log << "Blinker::blink(" << static_cast<uint32_t>(signal) << ")\n";
  step_ = static_cast<uint8_t>(signal);
  step_end_ms_ = millis();
  update();
}

void Blinker::update() {

  static constexpr struct {
    uint16_t duration_ms;
    bool is_on;
    uint8_t next_step;
  } STATES[] = {
      {0, false, kIdleStep},
      {100, true, 0},
      {100, true, 4},
      {0, true, kIdleStep},
      {1000, false, 2},
  };

  uint32_t now_ms = millis();
  while (step_ < std::size(STATES)) {

    if (now_ms - step_end_ms_ > std::numeric_limits<int32_t>::max()) {
      return;
    }

    auto state = STATES[step_];

    led_.set(state.is_on ? 1.0f : 0.0f);

    step_ = state.next_step;
    step_end_ms_ = now_ms + state.duration_ms;
  }
}

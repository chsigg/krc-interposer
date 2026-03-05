#include "Blinker.h"
#include "DigitalWritePin.h"
#include "Logger.h"
#include <cstdint>
#include <iterator>
#include <limits>

extern "C" uint32_t millis();

Blinker::Blinker(DigitalWritePin &led) : led_(led) {}

void Blinker::blink(Signal signal) {
  Log << "Blinker::blink(" << static_cast<uint32_t>(signal) << ")\n";
  step_ = static_cast<uint8_t>(signal);
  step_end_ms_ = millis();
  update();
}

void Blinker::update() {

  static constexpr struct {
    uint16_t duration_ms;
    PinState pin_state;
    uint8_t next_step;
  } STATES[] = {
      {0, PinState::High, kIdleStep},
      {100, PinState::Low, 0},
      {100, PinState::Low, 4},
      {0, PinState::Low, kIdleStep},
      {1000, PinState::High, 2},
  };

  uint32_t now_ms = millis();
  while (step_ < std::size(STATES)) {

    if (now_ms - step_end_ms_ > std::numeric_limits<int32_t>::max()) {
      return;
    }

    auto state = STATES[step_];

    led_.set(state.pin_state);

    step_ = state.next_step;
    step_end_ms_ = now_ms + state.duration_ms;
  }
}

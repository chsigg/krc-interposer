#include "Blinker.h"
#include "DigitalWritePin.h"
#include "Logger.h"
#include <cstdint>
#include <iterator>

extern "C" uint32_t millis();

Blinker::Blinker(DigitalWritePin &led) : led_(led) {}

void Blinker::blink(Signal signal) {
  Log << "Blinker::blink(" << static_cast<uint32_t>(signal) << ")\n";
  step_ = static_cast<uint8_t>(signal);
  next_step_time_ms_ = millis();
}

void Blinker::update() {

  static constexpr struct {
    uint16_t duration_ms;
    PinState pin_state;
    uint8_t next_step;
  } STATES[] = {
      // None
      {0, PinState::High, 4},
      // Once
      {100, PinState::Low, 0},
      // Repeat
      {100, PinState::Low, 3},
      {1000, PinState::High, 2},
  };

  if (step_ >= std::size(STATES) || millis() < next_step_time_ms_) {
    return;
  }

  auto state = STATES[step_];

  led_.set(state.pin_state);
  step_ = state.next_step;
  next_step_time_ms_ = millis() + state.duration_ms;
}

#include "Blinker.h"
#include "DigitalWritePin.h"
#include <cstdint>

extern "C" uint32_t millis();

Blinker::Blinker(DigitalWritePin &led) : led_(led) {}

void Blinker::blink(Signal signal) {
  static constexpr uint32_t FIRST_STEPS[] = {0, 1, 3};
  step_ = FIRST_STEPS[static_cast<uint32_t>(signal)];
  next_step_time_ms_ = millis();
}

void Blinker::update() {
  if (millis() < next_step_time_ms_) {
    return;
  }

  static constexpr struct {
    union {
      PinState state;
      uint8_t step;
    };
    uint16_t duration;
  } BLINKER_STEPS[] = {
      // None
      {{.step = 0}, 0},
      // Once
      {PinState::Low, 100},
      {{.step = 0}, 0},
      // Repeat
      {PinState::Low, 100},
      {PinState::High, 1000},
      {{.step = 3}, 0},
  };

  auto step = BLINKER_STEPS[step_];

  if (step.duration > 0) {
    led_.set(step.state);
  } else {
    led_.set(PinState::High);
  }
  if (step_ == 0) {
    return;
  }
  next_step_time_ms_ = millis() + step.duration;

  ++step_;
  step = BLINKER_STEPS[step_];
  if (step.duration == 0) {
    step_ = step.step;
  }
}

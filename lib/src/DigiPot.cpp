#include "DigiPot.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

void delayUs(uint32_t us);

DigiPot::DigiPot(const DigitalWritePin &inc, const DigitalWritePin &ud,
                 const DigitalWritePin &cs)
    : inc_(inc), ud_(ud), cs_(cs) {
  inc_.set(PinState::High);
  pulse(false, NUM_STEPS);
}

void DigiPot::setPosition(float position) {
  int32_t step = std::lround(std::clamp(position, 0.0f, 1.0f) * (NUM_STEPS - 1));

  if (step == current_step_) {
    return;
  }

  Log << "DigiPot::setPosition(" << position << ")\n";

  pulse(step > current_step_, std::abs(step - current_step_));
  current_step_ = step;
}

float DigiPot::getPosition() const { return current_step_ / (NUM_STEPS - 1.0f); }

void DigiPot::pulse(bool up, int32_t count) {
  ud_.set(up ? PinState::High : PinState::Low);
  delayUs(2);
  cs_.set(PinState::Low);
  delayUs(2);
  for (int32_t i = 0; i < count; ++i) {
    inc_.set(PinState::Low);
    delayUs(2);
    inc_.set(PinState::High);
    delayUs(2);
  }
  cs_.set(PinState::High);
  delayUs(2);
}

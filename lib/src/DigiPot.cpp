#include "DigiPot.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

#ifdef ARDUINO
#define USE_RGB_LED
#endif

#ifdef USE_RGB_LED
#include <Arduino.h>
#undef abs

static void setLedColor(float position) {
  float p = std::clamp(position, 0.0f, 1.0f);
  float r = 0.0f, g = 0.0f, b = 0.0f;

  if (p < 0.5f) {
    // Green (0.0) -> Blue (0.5)
    g = 1.0f - 2.0f * p;
    b = 2.0f * p;
  } else {
    // Blue (0.5) -> Red (1.0)
    b = 2.0f * (1.0f - p);
    r = 2.0f * (p - 0.5f);
  }

  // Write PWM values to the pins (D1=Red, D2=Green, D3=Blue)
  analogWrite(D3, static_cast<int>(r * 255));
  analogWrite(D1, static_cast<int>(g * 255));
  analogWrite(D2, static_cast<int>(b * 255));
}
#endif

void delayUs(uint32_t us);

DigiPot::DigiPot(const DigitalWritePin &inc, const DigitalWritePin &ud,
                 const DigitalWritePin &cs)
    : inc_(inc), ud_(ud), cs_(cs) {
  inc_.set(PinState::High);
  pulse(false, NUM_STEPS);
}

void DigiPot::setPosition(float position) {
  int32_t step =
      std::lround(std::clamp(position, 0.0f, 1.0f) * (NUM_STEPS - 1));

  if (step == current_step_) {
    return;
  }

  if (std::abs(step - printed_step_) > 5) {
    Log << "DigiPot::setPosition(" << position << ")\n";
    printed_step_ = step;
  }

  pulse(step > current_step_, std::abs(step - current_step_));
  current_step_ = step;

#ifdef USE_RGB_LED
  setLedColor(position);
#endif
}

float DigiPot::getPosition() const {
  return current_step_ / (NUM_STEPS - 1.0f);
}

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

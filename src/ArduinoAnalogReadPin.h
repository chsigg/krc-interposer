#pragma once

#include "AnalogReadPin.h"
#include <Arduino.h>

class ArduinoAnalogReadPin : public AnalogReadPin {
public:
  explicit ArduinoAnalogReadPin(int pin, float scale)
      : pin_(pin), scale_(scale) {
    pinMode(pin_, INPUT);
  }

  float read() const override { return analogRead(pin_) * scale_; }

private:
  const int pin_;
  const float scale_;
};

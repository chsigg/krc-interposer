#pragma once

#include "AnalogReadPin.h"
#include <Arduino.h>

class ArduinoAnalogReadPin final : public AnalogReadPin {
public:
  explicit ArduinoAnalogReadPin(int pin) : pin_(pin) {}

  virtual void begin() {
    pinMode(pin_, INPUT);
  }

  float read() const override { return analogRead(pin_); }

private:
  const int pin_;
};

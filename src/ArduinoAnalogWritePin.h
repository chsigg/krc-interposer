#pragma once

#include <Arduino.h>
#include <algorithm>

class ArduinoAnalogWritePin final {
public:
  explicit ArduinoAnalogWritePin(int pin) : pin_(pin) {}

  virtual void begin() {
    pinMode(pin_, OUTPUT);
  }

  virtual void write(float value) {
    int pwm = static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
    analogWrite(pin_, pwm);
  }

private:
  const int pin_;
};

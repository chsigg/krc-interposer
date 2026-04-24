#pragma once

#include <algorithm>

#include <Arduino.h>

#include "Led.h"

class ArduinoLed final : public Led {
public:
    explicit ArduinoLed(int pin) : pin_(pin) {}

    virtual void begin() {
        pinMode(pin_, OUTPUT);
    }

    void set(float value) const override {
        if (value <= 0.0f) {
            return digitalWrite(pin_, HIGH);
        }
        if (value >= 1.0f) {
            return digitalWrite(pin_, LOW);
        }
        static constexpr int kScale = (1 << ADC_RESOLUTION) - 1;
        analogWrite(pin_, std::clamp<int>((1.0f - value) * kScale, 0, kScale));
    }

private:
    const int pin_;
};

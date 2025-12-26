#pragma once

#include <Arduino.h>
#include "DigitalWritePin.h"

class ArduinoDigitalWritePin final : public DigitalWritePin {
public:
    explicit ArduinoDigitalWritePin(int pin) : pin_(pin) {}

    virtual void begin() {
        pinMode(pin_, OUTPUT);
    }

    void set(PinState state) const override {
        digitalWrite(pin_, state == PinState::High ? HIGH : LOW);
    }

private:
    const int pin_;
};

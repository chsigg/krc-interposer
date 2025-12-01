#pragma once

#include <Arduino.h>
#include "DigitalWritePin.h"

class ArduinoDigitalWritePin : public DigitalWritePin {
public:
    explicit ArduinoDigitalWritePin(int pin) : pin_(pin) {
        pinMode(pin_, OUTPUT);
    }

    void set(PinState state) override {
        digitalWrite(pin_, state == PinState::High ? HIGH : LOW);
    }

private:
    int pin_;
};

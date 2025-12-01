#pragma once

#include <Arduino.h>
#include "AnalogReadPin.h"

class ArduinoAnalogReadPin : public AnalogReadPin {
public:
    explicit ArduinoAnalogReadPin(int pin) : pin_(pin) {
        pinMode(pin_, INPUT);
    }

    int read() override {
        return analogRead(pin_);
    }

private:
    int pin_;
};

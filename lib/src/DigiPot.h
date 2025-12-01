#pragma once

#include "DigitalWritePin.h"

class DigiPot {
public:
    static constexpr int NUM_STEPS = 100;

    DigiPot(DigitalWritePin& inc, DigitalWritePin& ud, DigitalWritePin& cs);

    void setLevel(float level);
    float getLevel() const;

private:
    void pulse(bool up, int count);

    DigitalWritePin& inc_;
    DigitalWritePin& ud_;
    DigitalWritePin& cs_;
    int current_step_;
};

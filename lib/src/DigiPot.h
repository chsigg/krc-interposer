#pragma once

#include "DigitalWritePin.h"

class DigiPot {
public:
    static constexpr int NUM_STEPS = 100;

    DigiPot(const DigitalWritePin& inc, const DigitalWritePin& ud, const DigitalWritePin& cs);

    void setLevel(float level);
    float getLevel() const;

private:
    void pulse(bool up, int count);

    const DigitalWritePin& inc_;
    const DigitalWritePin& ud_;
    const DigitalWritePin& cs_;
    int current_step_;
};

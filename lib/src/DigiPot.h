#pragma once

#include "DigitalWritePin.h"
#include <cstdint>

class DigiPot {
public:
    static constexpr int32_t NUM_STEPS = 100;

    DigiPot(const DigitalWritePin& inc, const DigitalWritePin& ud, const DigitalWritePin& cs);
    virtual ~DigiPot() = default;

    virtual void setLevel(float level);
    virtual float getLevel() const;

private:
    void pulse(bool up, int32_t count);

    const DigitalWritePin& inc_;
    const DigitalWritePin& ud_;
    const DigitalWritePin& cs_;

    int32_t current_step_ = 0;
};

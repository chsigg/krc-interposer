#pragma once

enum class PinState {
    Low,
    High
};

class DigitalWritePin {
public:
    virtual ~DigitalWritePin() = default;

    virtual void set(PinState state) = 0;
};

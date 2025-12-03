#pragma once

#include <cstdint>

class AnalogReadPin {
public:
    virtual ~AnalogReadPin() = default;

    virtual float read() const = 0;
};

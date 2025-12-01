#pragma once

#include <cstdint>

class AnalogReadPin {
public:
    virtual ~AnalogReadPin() = default;

    virtual int32_t read() const = 0;
};

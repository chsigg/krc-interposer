#pragma once

#include <cstdint>

class DialSensor {
public:
    virtual ~DialSensor() = default;

    virtual float read() const = 0;
};

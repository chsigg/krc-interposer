#pragma once

class AnalogReadPin {
public:
    virtual ~AnalogReadPin() = default;

    virtual int read() const = 0;
};

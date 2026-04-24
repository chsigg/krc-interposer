#pragma once

class StoveActuator {
public:
    virtual ~StoveActuator() = default;
    virtual void write(float value) = 0;
};

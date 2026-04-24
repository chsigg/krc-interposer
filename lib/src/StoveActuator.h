#pragma once

class StoveActuator {
public:
    virtual ~StoveActuator() = default;
    virtual void setPwm(float value) = 0;
    virtual void setPassthrough() = 0;
};

#pragma once

class Potentiometer {
public:
    virtual ~Potentiometer() = default;

    virtual void setValue(float value) = 0;
};

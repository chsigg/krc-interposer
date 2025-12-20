#pragma once

class Potentiometer {
public:
    virtual ~Potentiometer() = default;

    virtual void setPosition(float position) = 0;
};

#pragma once

class Led {
public:
    virtual ~Led() = default;
    virtual void set(float value) const = 0;
};

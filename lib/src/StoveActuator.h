#pragma once

#include "DigiPot.h"
#include "StoveThrottle.h"
#include <cstdint>

class StoveActuator {
public:
    StoveActuator(DigiPot& pot, const ThrottleConfig& config);

    virtual void setThrottle(const StoveThrottle& throttle);
    StoveThrottle getThrottle() const { return target_throttle_; }

    void update();

private:
    DigiPot& pot_;
    const ThrottleConfig config_;

    StoveThrottle target_throttle_ = {};
    uint32_t current_boost_ = 0;
    uint32_t last_boost_change_ms_ = 0;
};

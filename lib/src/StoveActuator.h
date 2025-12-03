#pragma once

#include "DigiPot.h"
#include "StoveLevel.h"
#include <cstdint>

class StoveActuator {
public:
    StoveActuator(DigiPot& pot, const StoveLevelConfig& config);

    void setLevel(const StoveLevel& level);

    void update();

private:
    DigiPot& pot_;
    const StoveLevelConfig& config_;

    StoveLevel target_level_ = {};
    int32_t current_boost_ = 0;
};

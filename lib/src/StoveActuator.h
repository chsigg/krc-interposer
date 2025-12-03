#pragma once

#include "DigiPot.h"
#include "StoveLevel.h"
#include <cstdint>

class StoveActuator {
public:
    StoveActuator(DigiPot& pot, const LevelConfig& config);

    virtual void setLevel(const StoveLevel& level);
    StoveLevel getLevel() const { return target_level_; }

    void update();

private:
    DigiPot& pot_;
    const LevelConfig config_;

    StoveLevel target_level_ = {};
    uint32_t current_boost_ = 0;
};

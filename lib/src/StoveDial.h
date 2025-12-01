#pragma once

#include "AnalogReadPin.h"
#include <array>
#include <cstdint>

struct StoveDialConfig {
    int32_t min;
    int32_t max;
    int32_t boost;
    int32_t num_boosts;
};

class StoveDial {
public:
    StoveDial(const AnalogReadPin& pin, const StoveDialConfig& config);

    void update();

    float getLevel() const;
    int32_t getBoostLevel() const;

private:
    const AnalogReadPin& pin_;
    const StoveDialConfig config_;

    int32_t current_boost_ = 0;
    float current_level_ = 0.0f;

    std::array<int32_t, 4> last_readings_ = {};
    bool boost_armed_ = true;
};

#pragma once

#include "AnalogReadPin.h"
#include <array>
#include <cstdint>

struct StoveDialConfig {
    int min;
    int max;
    int boost;
    int num_boosts;
};

class StoveDial {
public:
    StoveDial(AnalogReadPin& pin, const StoveDialConfig& config);

    void update();

    float getLevel() const;
    int getBoostLevel() const;

private:
    AnalogReadPin& pin_;
    StoveDialConfig config_;

    int current_boost_ = 0;
    float current_level_ = 0.0f;

    std::array<int, 4> last_readings_ = {};
    bool boost_armed_ = true;
};

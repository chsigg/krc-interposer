#include "AdafruitPotentiometer.h"
#include <Arduino.h>
#include <Wire.h>
#include <algorithm>

void AdafruitPotentiometer::begin() {
    ds3502_.begin();
    last_wiper_ = ds3502_.getWiper();
}

void AdafruitPotentiometer::setPosition(float position) {
    // Map 0.0-1.0 to 0-127
    float clamped = std::clamp(position, 0.0f, 1.0f);
    uint8_t wiper = static_cast<uint8_t>(clamped * 127.0f);

    if (wiper == last_wiper_) {
        return;
    }

    ds3502_.setWiper(wiper);
    last_wiper_ = wiper;
}

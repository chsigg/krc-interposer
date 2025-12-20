#pragma once

#include "Potentiometer.h"
#include <Adafruit_DS3502.h>

class AdafruitPotentiometer : public Potentiometer {
public:
    void begin();
    void setPosition(float position) override;

private:
    Adafruit_DS3502 ds3502_;
    int last_wiper_ = 0;
};

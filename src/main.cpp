#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "ArduinoAnalogReadPin.h"
#include "ArduinoBuzzer.h"
#include "ArduinoDigitalWritePin.h"
#include "Beeper.h"
#include "Buzzer.h"
#include "DigiPot.h"
#include "StoveDial.h"

ArduinoBuzzer buzzer(10, 11);
Beeper beeper(buzzer);
ArduinoDigitalWritePin inc(2), ud(3), cs(4);
DigiPot pot(inc, ud, cs);
ArduinoAnalogReadPin read_pin(1);
StoveDial dial(read_pin, {10, 100, 150, 2});

void setup() {
    Serial.begin(115200);
}

void loop() {
    dial.update();
    beeper.update();
    pot.setLevel(dial.getLevel());
}

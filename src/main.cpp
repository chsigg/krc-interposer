#include "AnalogReadPin.h"
#include "ArduinoAnalogReadPin.h"
#include "ArduinoBuzzer.h"
#include "ArduinoDigitalWritePin.h"
#include "Beeper.h"
#include "Buzzer.h"
#include "DigiPot.h"
#include "DigitalWritePin.h"
#include "StoveActuator.h"
#include "StoveDial.h"
#include "StoveLevel.h"
#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

void delayUs(uint32_t us) { delayMicroseconds(us); }

ArduinoBuzzer buzzer(10, 11);
Beeper beeper(buzzer);

StoveLevelConfig config;

ArduinoAnalogReadPin read_pin(1, 1.0f / 1023);
StoveDial dial(read_pin, config);

ArduinoDigitalWritePin inc(2), ud(3), cs(4);
DigiPot pot(inc, ud, cs);
StoveActuator actuator(pot, config);

void setup() {
  Serial.begin(115200);
  actuator.setLevel(dial.getLevel());
}

void loop() {
  beeper.update();
  dial.update();
  actuator.update();
}

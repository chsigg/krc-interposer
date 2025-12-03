#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <bluefruit.h>

#include "BleShutterClient.h"
#include "BleTemperatureClient.h"
#include "StoveSupervisor.h"

// Hardware Drivers
#include "ArduinoAnalogReadPin.h"
#include "ArduinoBuzzer.h"
#include "ArduinoDigitalWritePin.h"
#include "Beeper.h"
#include "StoveActuator.h"
#include "StoveDial.h"
#include "ThermalController.h"
#include "TrendAnalyzer.h"

void delayUs(uint32_t us) { delayMicroseconds(us); }

// --- Hardware Instantiation ---

// Actuator Pins
ArduinoDigitalWritePin inc(D1), ud(D2), cs(D3);
DigiPot pot(inc, ud, cs);
LevelConfig level_config; // Defaults
StoveActuator actuator(pot, level_config);

// Sensor Pins
ArduinoAnalogReadPin read_pin(A0, 1.0f / 1023.0f);
StoveDial dial(read_pin, level_config);

// Audio
ArduinoBuzzer buzzer(D5, D8);
Beeper beeper(buzzer);

// Logic Modules
TrendAnalyzer analyzer;
ThermalConfig thermal_config; // Defaults
ThermalController controller(analyzer, thermal_config);

// Supervisor
StoveConfig stove_config;
StoveSupervisor supervisor(dial, actuator, controller, beeper, analyzer, stove_config, level_config);

// BLE Modules
BleTemperatureClient bleTemp(supervisor, analyzer);
BleShutterClient bleShutter(supervisor);


void setup() {
  Serial.begin(115200);
  Serial.println("Stove Controller Starting...");
  pinMode(LED_RED, OUTPUT);
}

void loop() {
  // Update hardware/logic
  beeper.update();
  dial.update();
  controller.update(); // calculates PID based on analyzer

  // Run supervisor logic (Snapshot feedback, Safety, Mixing)
  supervisor.update();

  // Actuator update (actually sends to Pot)
  actuator.update();

  static uint32_t last_log = 0;
  static bool led_state = false;
  if (millis() - last_log > 1000) {
    last_log = millis();
    Serial.println("");

    Serial.print("Dial: level ");
    Serial.print(dial.getLevel().base);
    Serial.print(", boost ");
    Serial.println(dial.getLevel().boost);

    Serial.print("Analyzer: ");
    Serial.print(analyzer.getValue(millis()));
    Serial.print("°C ");
    Serial.print(analyzer.getSlope());
    Serial.println("°C/ms ");

    Serial.print("Controller: level ");
    Serial.print(controller.getLevel());
    Serial.print(", lid open ");
    Serial.println(controller.isLidOpen());

    Serial.print("Actuator: level ");
    Serial.print(actuator.getLevel().base);
    Serial.print(", boost ");
    Serial.println(actuator.getLevel().boost);

    led_state = !led_state;
    digitalWrite(LED_RED, led_state ? HIGH : LOW);
  }

  delay(10);
}

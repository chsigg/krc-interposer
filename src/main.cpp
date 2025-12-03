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
ArduinoDigitalWritePin inc(2), ud(3), cs(4);
DigiPot pot(inc, ud, cs);
LevelConfig level_config; // Defaults
StoveActuator actuator(pot, level_config);

// Sensor Pins
ArduinoAnalogReadPin read_pin(A0, 1.0f / 1023.0f);
StoveDial dial(read_pin, level_config);

// Audio
ArduinoBuzzer buzzer(10, 11);
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

  delay(10);
}

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <Streaming.h>
#include <bluefruit.h>

#include "ArduinoAnalogReadPin.h"
#include "ArduinoBuzzer.h"
#include "ArduinoDigitalWritePin.h"
#include "ArduinoLogger.h"
#include "Beeper.h"
#include "BleShutterClient.h"
#include "BleTelemetry.h"
#include "BleTemperatureClient.h"
#include "Blinker.h"
#include "StoveActuator.h"
#include "StoveDial.h"
#include "StoveSupervisor.h"
#include "Streaming.h"
#include "ThermalController.h"
#include "TrendAnalyzer.h"

void delayUs(uint32_t us) { delayMicroseconds(us); }

/* TODOs
- turn off radio when stove dial is 0
- when stove dial is turned on, turn on radio
- ignore if lid is available within first 3 seconds
- initial connect takes snapshot, reconnect doesn't
- use shutter trigger to force snapshot
*/

// --- Hardware Instantiation ---

// BLE UART Service
BLEUart bleuart;
// Tee stream for logging to Serial and BLE
ArduinoLogger logger(Serial, bleuart);
Logger &Log = logger;

// Actuator Pins
ArduinoDigitalWritePin inc(D1), ud(D2), cs(D3);
DigiPot digi_pot(inc, ud, cs);
ThrottleConfig throttle_config; // Defaults
StoveActuator actuator(digi_pot, throttle_config);

// Sensor Pins
ArduinoAnalogReadPin read_pin(A0, 1.0f / (1023.0f * 0.85f));
StoveDial dial(read_pin, throttle_config);

// Feedback
ArduinoBuzzer buzzer(D7, D9);
Beeper beeper(buzzer);
ArduinoDigitalWritePin red_led(LED_RED);
Blinker blinker(red_led);

// Logic Modules
TrendAnalyzer analyzer;
ThermalConfig thermal_config; // Defaults
ThermalController controller(analyzer, thermal_config);

// Supervisor
StoveConfig stove_config;
StoveSupervisor supervisor(dial, actuator, controller, beeper, analyzer,
                           stove_config, throttle_config);

// BLE Modules
BleTemperatureClient temp_client(supervisor, analyzer);
BleShutterClient shutter_client(supervisor);
BleTelemetry telemetry(bleuart, controller, analyzer);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    delay(10);
  }

  Log << "KRC Interceptor Starting...\n";

  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.Security.setIOCaps(false, false, false);
  Bluefruit.Security.setMITM(false);

  // Initialize BLE Central clients
  BleClient::begin();

  // Setup BLE Peripheral and start advertising
  telemetry.begin();

  blinker.blink(Blinker::Signal::REPEAT);
}

static void log(uint32_t time_ms) {
  static uint32_t last_log_ms = 0;
  if (time_ms - last_log_ms < 60 * 1000) {
    return;
  }
  last_log_ms = time_ms;

  Log << "Analyzer: " << analyzer.getValue(last_log_ms) << "°C "
      << analyzer.getSlope() << "°C/ms\n";
  Log << "Dial: throttle " << dial.getThrottle().base << ", boost "
      << dial.getThrottle().boost << "\n";
  Log << "Controller: level " << controller.getLevel()
      << (controller.isLidOpen() ? " (lid open)" : "") << "\n";
  Log << "Actuator: throttle " << actuator.getThrottle().base << ", boost "
      << actuator.getThrottle().boost << "\n\n";
}

void loop() {
  uint32_t now = millis();

  beeper.update();
  blinker.update();

  dial.update();
  controller.update();
  supervisor.update();
  actuator.update();

  static uint32_t throttle_start_ms = now;
  if (dial.getPosition() < throttle_config.min) {
    throttle_start_ms = now;
  }
  BleClient::update(now - throttle_start_ms >= 2000);

  log(now);

  telemetry.update();

  delay(10);
}

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <Wire.h>
#include <bluefruit.h>

#include "AdafruitPotentiometer.h"
#include "ArduinoAnalogReadPin.h"
#include "ArduinoBuzzer.h"
#include "ArduinoDigitalWritePin.h"
#include "ArduinoLogger.h"
#include "Beeper.h"
#include "BleClient.h"
#include "BleShutterClient.h"
#include "BleTelemetry.h"
#include "BleTemperatureClient.h"
#include "Blinker.h"
#include "StoveActuator.h"
#include "StoveDial.h"
#include "StoveSupervisor.h"
#include "ThermalController.h"
#include "TrendAnalyzer.h"

void delayUs(uint32_t us) { delayMicroseconds(us); }

constexpr int kStoveDialPin = A4;
constexpr int kBypassPin = D10;
constexpr int kSdaPin = D3;
constexpr int kSclPin = D2;
constexpr int kBuzzerPPin = D0;
constexpr int kBuzzerNPin = D1;
constexpr int kLedPin = LED_RED;

// --- Hardware Instantiation ---

// BLE UART Service
BLEUart bleuart;
// Tee stream for logging to Serial and BLE
ArduinoLogger logger(Serial, bleuart);
Logger &Log = logger;

// Actuator Pins
ArduinoDigitalWritePin bypass_pin(kBypassPin);
AdafruitPotentiometer potentiometer;
ThrottleConfig throttle_config; // Defaults
StoveActuator actuator(potentiometer, bypass_pin, throttle_config);

// Sensor Pins
ArduinoAnalogReadPin read_pin(kStoveDialPin, 1.0f / (1023.0f * 0.85f));
StoveDial dial(read_pin, throttle_config);

// Feedback
ArduinoBuzzer buzzer(kBuzzerPPin, kBuzzerNPin);
Beeper beeper(buzzer);
ArduinoDigitalWritePin red_led(kLedPin);
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

  analogReadResolution(12);
  actuator.setBypass();
  Wire.setPins(kSdaPin, kSclPin);
  potentiometer.begin();

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

  if (analyzer.getLastUpdateMs() != 0) {
    Log << "Analyzer: " << analyzer.getValue(last_log_ms) << "°C "
        << analyzer.getSlope() << "°C/ms\n";
  }
  Log << "Dial: throttle " << dial.getThrottle().base << ", boost "
      << dial.getThrottle().boost << "\n";
  Log << "Controller: level " << controller.getLevel()
      << (controller.isLidOpen() ? " (lid open)" : "") << "\n";
}

void start() { BleClient::start(); }

void stop() { BleClient::stop(); }

void loop() {
  uint32_t now = millis();

  beeper.update();
  blinker.update();

  supervisor.update();

  static uint32_t dial_off_ms = now;
  if (dial.isOff()) {
    dial_off_ms = now;
  }
  static bool started = false;
  if (bool should_start = now - dial_off_ms >= 2000; should_start != started) {
    started = should_start;
    should_start ? start() : stop();
  }

  log(now);

  telemetry.update();

  delay(10);
}

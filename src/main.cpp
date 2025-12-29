#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <Wire.h>
#include <algorithm>
#include <bluefruit.h>

#include "AdafruitPotentiometer.h"
#include "ArduinoAnalogReadPin.h"
#include "ArduinoAnalogWritePin.h"
#include "ArduinoBuzzer.h"
#include "ArduinoDigitalWritePin.h"
#include "ArduinoLogger.h"
#include "Beeper.h"
#include "BleTelemetry.h"
#include "BleThermometer.h"
#include "StoveActuator.h"
#include "StoveDial.h"
#include "StoveSupervisor.h"
#include "ThermalController.h"
#include "TrendAnalyzer.h"

void delayUs(uint32_t us) { delayMicroseconds(us); }

constexpr int kBuzzerPPin = D0;
constexpr int kBuzzerNPin = D1;
constexpr int kSclPin = D2;
constexpr int kSdaPin = D3;
constexpr int kStoveDialPin = A4;
constexpr int kOutputReadPin = A5;
constexpr int kBypassPin = D10;
constexpr int kLedRedPin = LED_RED;
constexpr int kLedGreenPin = LED_GREEN;
constexpr int kLedBluePin = LED_BLUE;

// --- Hardware Instantiation ---

// Tee stream for logging to Serial and BLE
BLEUart bleuart;
ArduinoLogger logger(Serial, bleuart);
Logger &Log = logger;

// Actuator Pins
class BypassPin : public DigitalWritePin {
public:
  void begin() {
    write_pin_.begin();
    led_pin_.begin();
  }

  void set(PinState state) const override {
    write_pin_.set(state);
    led_pin_.set(state == PinState::High ? PinState::Low : PinState::High);
  }

private:
  ArduinoDigitalWritePin write_pin_{kBypassPin};
  ArduinoDigitalWritePin led_pin_{kLedGreenPin};
};

AdafruitPotentiometer potentiometer;
BypassPin bypass_pin;
ThrottleConfig throttle_config; // Defaults
StoveActuator actuator(potentiometer, bypass_pin, throttle_config);

// Sensor Pins
ArduinoAnalogReadPin input_read_pin(kStoveDialPin, 1.0f / 4095.0f / 0.9f);
StoveDial dial(input_read_pin, throttle_config);

// Feedback
ArduinoBuzzer buzzer(NRF_PWM3, kBuzzerPPin, kBuzzerNPin);
Beeper beeper(buzzer);
ArduinoAnalogReadPin output_read_pin(kOutputReadPin, 1.0f / 4095.0f);
ArduinoAnalogWritePin output_led_pin(kLedRedPin);

// Logic Modules
TrendAnalyzer analyzer;
ThermalConfig thermal_config; // Defaults
ThermalController controller(analyzer, thermal_config);

// BLE Modules
BleThermometer thermometer(analyzer);
BleTelemetry telemetry(bleuart, controller, analyzer);

// Supervisor
StoveConfig stove_config;
StoveSupervisor supervisor(dial, actuator, controller, beeper, analyzer,
                           thermometer, stove_config, throttle_config);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    delay(10);
  }

  Log << "KRC Interceptor Starting...\n";

  analogReadResolution(12);
  Wire.setPins(kSdaPin, kSclPin);
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.Security.setIOCaps(false, false, false);
  Bluefruit.Security.setMITM(false);

  bypass_pin.begin();
  input_read_pin.begin();
  output_read_pin.begin();
  output_led_pin.begin();
  buzzer.begin();
  potentiometer.begin();
  thermometer.begin();
  telemetry.begin();

  actuator.setBypass();
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

void loop() {
  uint32_t now = millis();

  supervisor.update();

  float output_val = std::clamp(output_read_pin.read(), 0.0f, 1.0f);
  output_led_pin.write(1.0f - output_val);

  static uint32_t dial_off_ms = now;
  if (dial.isOff()) {
    dial_off_ms = now;
  }

  log(now);
  telemetry.update();

  delay(10);
}

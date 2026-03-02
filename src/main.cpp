#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <algorithm>
#include <bluefruit.h>
#include <nrf_lpcomp.h>

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

constexpr int kStovePwmPin = A0;
constexpr int kBuzzerPPin = A1;
constexpr int kBuzzerNPin = A2;
constexpr int kDialReadPin = A3;
constexpr int kDialRefPin = A4;
constexpr int kBypassPin = D10;

class DialReadPin : public AnalogReadPin {
public:
  void begin() {
    read_pin_.begin();
    ref_pin_.begin();
  }

  float read() const override {
    float ref = ref_pin_.read();
    return ref == 0.0f ? 0.0f : read_pin_.read() / ref;
  }

private:
  ArduinoAnalogReadPin read_pin_{kDialReadPin, 1.0f};
  ArduinoAnalogReadPin ref_pin_{kDialRefPin, 1.0f};
};

// --- Hardware Instantiation ---

// Tee stream for logging to Serial and BLE
BLEUart bleuart;
ArduinoLogger logger(Serial, bleuart);
Logger &Log = logger;

// Actuators
ArduinoAnalogWritePin stove_pwm(kStovePwmPin);
ArduinoDigitalWritePin bypass_pin(kBypassPin);
ThrottleConfig throttle_config; // Defaults
StoveActuator actuator(stove_pwm, bypass_pin, throttle_config);

// Sensors
DialReadPin dial_read_pin;
StoveDial dial(dial_read_pin, throttle_config);

// Feedback
ArduinoBuzzer buzzer(NRF_PWM3, kBuzzerPPin, kBuzzerNPin);
Beeper beeper(buzzer);
ArduinoAnalogWritePin input_led_pin(LED_RED);

// Logic Modules
TrendAnalyzer analyzer;
ThermalConfig thermal_config; // Defaults
ThermalController controller(analyzer, thermal_config);

// BLE Modules
BleThermometer thermometer(analyzer);
BleTelemetry telemetry(bleuart, controller, analyzer);

static void poweroff() {
  Log << "Powering off...\n";
  Serial.end();
  thermometer.end();
  telemetry.end();

  for (int pin : {kBuzzerPPin, kBuzzerNPin}) {
    pinMode(pin, INPUT_PULLDOWN);
  }
  for (int pin : {kStovePwmPin, kDialReadPin, kDialRefPin, kBypassPin, LED_RED,
                  LED_GREEN, LED_BLUE}) {
    pinMode(pin, INPUT);
  }

  // Set up boot trigger when pin is 7/8 of VDD.
  nrf_lpcomp_disable(NRF_LPCOMP);
  const nrf_lpcomp_config_t lpcomp_config = {
      .reference = NRF_LPCOMP_REF_SUPPLY_7_8,
      .detection = NRF_LPCOMP_DETECT_UP,
      .hyst = NRF_LPCOMP_HYST_ENABLED};
  nrf_lpcomp_configure(NRF_LPCOMP, &lpcomp_config);
  nrf_lpcomp_input_select(NRF_LPCOMP, NRF_LPCOMP_INPUT_3);
  nrf_lpcomp_enable(NRF_LPCOMP);
  nrf_lpcomp_task_trigger(NRF_LPCOMP, NRF_LPCOMP_TASK_START);
  while (!nrf_lpcomp_event_check(NRF_LPCOMP, NRF_LPCOMP_EVENT_READY)) {
  } // Wait for ready

  sd_power_system_off();
}

// Supervisor
StoveConfig stove_config;
StoveSupervisor supervisor(dial, actuator, controller, beeper, analyzer,
                           thermometer, stove_config, throttle_config,
                           poweroff);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    delay(10);
  }

  Log << "KRC Interceptor Starting...\n";

  for (int pin : {D5, D6, D7, D8, D9}) {
    pinMode(pin, INPUT_PULLDOWN);
  }

  analogReadResolution(12);

  bypass_pin.begin();
  bypass_pin.set(PinState::Low);

  dial_read_pin.begin();
  input_led_pin.begin();
  buzzer.begin();
  stove_pwm.begin();

  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin(1, 1);
  Bluefruit.setTxPower(8);
  Bluefruit.setName("KRC Interposer");
  Bluefruit.Security.setIOCaps(false, false, false);
  Bluefruit.Security.setMITM(false);

  thermometer.begin();
  telemetry.begin();

  supervisor.begin();
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
  Log << "Dial: position " << dial.getPosition() << "\n";
  Log << "Controller: power " << controller.getPower()
      << (controller.isLidOpen() ? " (lid open)" : "") << "\n";
}

void loop() {
  uint32_t now = millis();

  supervisor.update();
  float input_val = std::clamp(dial_read_pin.read(), 0.0f, 1.0f);
  input_led_pin.write(1.0f - input_val);

  log(now);
  telemetry.update();

  delay(100);
}

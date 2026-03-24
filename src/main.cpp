#include <bluefruit.h>
#include <nrf_lpcomp.h>
#include <nrf_pwm.h>

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
#include "TimestampLogger.h"
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
    if (ref > 1 << (ADC_RESOLUTION - 2)) {
      return read_pin_.read() / ref;
    }
    return 0.0f;
  }

private:
  ArduinoAnalogReadPin read_pin_{kDialReadPin};
  ArduinoAnalogReadPin ref_pin_{kDialRefPin};
};

static void shutdown();
static void poweroff();

// --- Hardware Instantiation ---

// Tee stream for logging to Serial and BLE
BufferedLogger buffered_logger(1024);
ArduinoLogger arduino_logger(buffered_logger);
TimestampLogger timestamp_logger(arduino_logger);
Logger &Log = timestamp_logger;

// Actuators
ArduinoAnalogWritePin stove_pwm(kStovePwmPin);
ArduinoDigitalWritePin bypass_pin(kBypassPin);
ThrottleConfig throttle_config; // Defaults
StoveActuator actuator(stove_pwm, throttle_config);

// Sensors
DialReadPin dial_read_pin;
StoveDial dial(dial_read_pin, throttle_config);

// Feedback
ArduinoBuzzer buzzer(NRF_PWM3, kBuzzerPPin, kBuzzerNPin);
Beeper beeper(buzzer);
ArduinoAnalogWritePin red_led(LED_RED);
ArduinoDigitalWritePin green_led(LED_GREEN);

// Logic Modules
TrendAnalyzer analyzer;
ThermalConfig thermal_config; // Defaults
ThermalController controller(analyzer, thermal_config);

// BLE Modules
BleThermometer thermometer(analyzer);
BleTelemetry telemetry(controller, analyzer, buffered_logger);

// Supervisor
StoveConfig stove_config;
StoveSupervisor supervisor(dial, actuator, controller, beeper, analyzer,
                           bypass_pin, stove_config, throttle_config, shutdown);

void setup() {
  bypass_pin.begin();
  bypass_pin.set(PinState::Low);
  stove_pwm.begin();
  stove_pwm.write(0.0f);

  for (int pin : {D5, D6, D7, D8, D9}) {
    pinMode(pin, INPUT_PULLDOWN);
  }

  analogReadResolution(ADC_RESOLUTION);
  analogWriteResolution(ADC_RESOLUTION);

  dial_read_pin.begin();
  red_led.begin();
  green_led.begin();
  green_led.set(PinState::Low);

  Serial.begin(115200);

  Bluefruit.configPrphConn(BLE_GATT_ATT_MTU_MAX, BLE_GAP_EVENT_LENGTH_DEFAULT,
                           BLE_GATTS_HVN_TX_QUEUE_SIZE_DEFAULT,
                           BLE_GATTC_WRITE_CMD_TX_QUEUE_SIZE_DEFAULT);
  Bluefruit.begin(1, 1);
  Bluefruit.autoConnLed(false);
  Bluefruit.setTxPower(8);
  Bluefruit.setName("KRC Interposer");

  while (dial_read_pin.read() < throttle_config.boil) {
    if (millis() > 10000) {
      Log << "Boil level not detected, powering off.\n";
      poweroff();
    }
  }

  buzzer.begin();
  beeper.beep(Beeper::Signal::POWER_ON);

  while (!Serial && millis() < 1000) {
    beeper.update();
    delay(10);
  }
  Log << "KRC Interceptor Starting...\n";

  thermometer.begin();
  telemetry.begin();
  green_led.set(PinState::High);
}

void loop() {
  thermometer.update();
  supervisor.update();
  red_led.write(1.0f - controller.getPower());
  telemetry.update();
  delay(20);
}

static void shutdown() {
  beeper.beep(Beeper::Signal::POWER_OFF);
  while (!beeper.isIdle()) {
    delay(10);
    beeper.update();
  }

  Log << "Powering off...\n";

  thermometer.end();
  telemetry.update();
  telemetry.end();

  poweroff();
}

static void poweroff() {
  Serial.flush();
  Serial.end();

  for (auto pwm : {NRF_PWM0, NRF_PWM1, NRF_PWM2, NRF_PWM3}) {
    nrf_pwm_disable(pwm);
  }
  for (int pin : {kBuzzerPPin, kBuzzerNPin}) {
    pinMode(pin, INPUT_PULLDOWN);
  }
  for (int pin : {kStovePwmPin, kDialReadPin, kDialRefPin, kBypassPin}) {
    pinMode(pin, INPUT);
  }
  for (int pin : {LED_RED, LED_GREEN, LED_BLUE}) {
    pinMode(pin, INPUT_PULLUP);
  }

  // Set up boot trigger when pin is 7/8 of VDD.
  nrf_lpcomp_disable(NRF_LPCOMP);
  const nrf_lpcomp_config_t lpcomp_config = {.reference =
                                                 NRF_LPCOMP_REF_SUPPLY_6_8,
                                             .detection = NRF_LPCOMP_DETECT_UP,
                                             .hyst = NRF_LPCOMP_HYST_ENABLED};
  nrf_lpcomp_configure(NRF_LPCOMP, &lpcomp_config);
  nrf_lpcomp_input_select(NRF_LPCOMP, NRF_LPCOMP_INPUT_5);
  nrf_lpcomp_enable(NRF_LPCOMP);
  nrf_lpcomp_task_trigger(NRF_LPCOMP, NRF_LPCOMP_TASK_START);
  while (!nrf_lpcomp_event_check(NRF_LPCOMP, NRF_LPCOMP_EVENT_READY)) {
  } // Wait for ready

  sd_power_system_off();
}

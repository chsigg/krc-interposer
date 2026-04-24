#include <bluefruit.h>
#include <nrf_lpcomp.h>
#include <nrf_pwm.h>

#include "ArduinoBuzzer.h"
#include "ArduinoLed.h"
#include "ArduinoLogger.h"
#include "ArduinoUart.h"
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

constexpr int kBuzzerPPin = A2;
constexpr int kBuzzerNPin = A3;
constexpr int kUartRxPin = D1;
constexpr int kUartTxPin = D0;

static void shutdown();
static void poweroff();

// --- Hardware Instantiation ---

// Tee stream for logging to Serial and BLE
BufferedLogger buffered_logger(1024);
ArduinoLogger arduino_logger(buffered_logger);
TimestampLogger timestamp_logger(arduino_logger);
Logger &Log = timestamp_logger;

ArduinoUart uart(kUartRxPin, kUartTxPin);
ThrottleConfig throttle_config; // Defaults

// Actuators
StoveActuator actuator(uart, throttle_config);

// Sensors
StoveDial dial(uart, throttle_config);

// Feedback
ArduinoBuzzer buzzer(NRF_PWM3, kBuzzerPPin, kBuzzerNPin);
Beeper beeper(buzzer);
ArduinoLed red_led(LED_RED);
ArduinoLed green_led(LED_GREEN);

// Logic Modules
TrendAnalyzer analyzer;
ThermalConfig thermal_config; // Defaults
ThermalController controller(analyzer, thermal_config);

// BLE Modules
BleThermometer thermometer(analyzer);
BleTelemetry telemetry(controller, analyzer, buffered_logger);

// Supervisor (bypassing is handled internally by uart)
StoveConfig stove_config;
StoveSupervisor supervisor(dial, actuator, controller, beeper, analyzer,
                           uart, stove_config, throttle_config, shutdown);

void setup() {
  uart.begin();
  uart.set(PinState::Low);

  for (int pin : {D4, D5, D6, D7, D8, D9, D10}) {
    pinMode(pin, INPUT_PULLDOWN);
  }

  analogWriteResolution(ADC_RESOLUTION);

  red_led.begin();
  green_led.begin();
  green_led.set(1.0f);

  Serial.begin(115200);

  Bluefruit.configPrphConn(BLE_GATT_ATT_MTU_MAX, BLE_GAP_EVENT_LENGTH_DEFAULT,
                           BLE_GATTS_HVN_TX_QUEUE_SIZE_DEFAULT,
                           BLE_GATTC_WRITE_CMD_TX_QUEUE_SIZE_DEFAULT);
  Bluefruit.begin(1, 1);
  Bluefruit.autoConnLed(false);
  Bluefruit.setTxPower(8);
  Bluefruit.setName("KRC Interposer");

  // Spin until we receive valid boil data from the PIC UART
  while (uart.read() < throttle_config.boil) {
    uart.update();
    if (millis() > 10000) {
      Log << "Boil level not detected, powering off.\n";
      poweroff();
    }
    delay(10);
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
  green_led.set(0.0f);
}

void loop() {
  uart.update();
  thermometer.update();
  supervisor.update();
  red_led.set(controller.getPower());
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
  Serial1.flush();
  Serial1.end();

  for (auto pwm : {NRF_PWM0, NRF_PWM1, NRF_PWM2, NRF_PWM3}) {
    nrf_pwm_disable(pwm);
  }
  for (int pin : {kBuzzerPPin, kBuzzerNPin}) {
    pinMode(pin, INPUT_PULLDOWN);
  }
  for (int pin : {LED_RED, LED_GREEN, LED_BLUE}) {
    pinMode(pin, INPUT_PULLUP);
  }

  // Set up System OFF Boot trigger based on UART RX Start Bit (High-to-Low)
  nrf_gpio_cfg_sense_input(g_ADigitalPinMap[kUartRxPin], NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
  sd_power_system_off();
}


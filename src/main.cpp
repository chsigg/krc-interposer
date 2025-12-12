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
DigiPot pot(inc, ud, cs);
ThrottleConfig throttle_config; // Defaults
StoveActuator actuator(pot, throttle_config);

// Sensor Pins
ArduinoAnalogReadPin read_pin(A0, 1.0f / 1023.0f);
StoveDial dial(read_pin, throttle_config);

// Feedback
ArduinoBuzzer buzzer(D5, D8);
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
BleTemperatureClient bleTemp(supervisor, analyzer);
BleShutterClient bleShutter(supervisor);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    delay(10);
  }
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Log << "KRC Interceptor Starting..."
      << "\n";

  // Initialize existing BLE Central clients
  BleClient::begin();

  // --- BLE Peripheral/UART Setup ---
  // Start the BLE UART service
  bleuart.bufferTXD(true);
  bleuart.begin();

  // Setup the advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  // Include the BLE UART (NUS) service UUID
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.Advertising.addName();

  // Configure and start advertising
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // 20ms, 152.5ms
  Bluefruit.Advertising.setFastTimeout(30);   // 30 seconds
  Bluefruit.Advertising.start(0);             // 0 = Advertise forever

  blinker.blink(Blinker::Signal::REPEAT);
}

void loop() {
  // Update hardware/logic
  beeper.update();
  blinker.update();
  dial.update();
  controller.update(); // calculates PID based on analyzer

  // Run supervisor logic (Snapshot feedback, Safety, Mixing)
  supervisor.update();

  // Actuator update (actually sends to Pot)
  actuator.update();

  static uint32_t last_log = 0;
  if (millis() - last_log > 1000) {
    last_log = millis();

    Log << "Analyzer: " << analyzer.getValue(last_log) << "°C "
        << analyzer.getSlope() << "°C/ms "
        << "\n";
    Log << "Dial: throttle " << dial.getThrottle().base << ", boost "
        << dial.getThrottle().boost << "\n";
    Log << "Controller: level " << controller.getLevel()
        << (controller.isLidOpen() ? " (lid open)" : "") << "\n";
    Log << "Actuator: throttle " << actuator.getThrottle().base << ", boost "
        << actuator.getThrottle().boost << "\n";
    Log << "\n";
  }

  bleuart.flushTXD();
  delay(10);
}

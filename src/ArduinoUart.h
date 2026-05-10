#include <Arduino.h>
#include <algorithm>
#include <nrf_gpio.h>

#include "DialSensor.h"
#include "StoveActuator.h"

class ArduinoUart : public DialSensor, public StoveActuator {
public:
  ArduinoUart(int rx_pin, int tx_pin) : rx_pin_(rx_pin), tx_pin_(tx_pin) {}

  void begin() {
    Serial1.setPins(rx_pin_, tx_pin_);
    Serial1.begin(9600, SERIAL_8E1); // 8 data bits, Even parity, 1 stop bit

    // Enable the internal 3.3V pull-up on RX pin to match the PIC's open-drain 5V TX pin.
    pinMode(rx_pin_, INPUT_PULLUP);
  }

  void end() {
    Serial1.flush();
    Serial1.end();
  }

  void update() {
    // 1. Send request/command to PIC
    uint8_t tx = std::clamp<int>(tx_value_ * 255, 0, 255);
    Serial1.write(tx);

    // 2. Wait for response (with a short timeout)
    uint32_t start_ms = millis();
    while (Serial1.available() == 0 && millis() - start_ms < 10) {
      // Wait up to 10ms for response
    }

    // 3. Read response if available
    if (Serial1.available() > 0) {
      uint8_t rx = Serial1.read();
      rx_value_ = rx * (1.0f / 255);
    }
  }

  float read() const override { return rx_value_; }
  void write(float value) override { tx_value_ = value; }

private:
  const int rx_pin_;
  const int tx_pin_;
  float rx_value_ = 1.0f;
  float tx_value_ = 0.0f;
};

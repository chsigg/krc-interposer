#include <Arduino.h>
#include <algorithm>
#include <nrf_gpio.h>

#include "AnalogReadPin.h"
#include "AnalogWritePin.h"
#include "DigitalWritePin.h"

class ArduinoUart : public AnalogReadPin, public AnalogWritePin, public DigitalWritePin {
public:
  ArduinoUart(int rx_pin, int tx_pin) : rx_pin_(rx_pin), tx_pin_(tx_pin) {}

  void begin() {
    Serial1.setPins(rx_pin_, tx_pin_);
    Serial1.begin(9600);

    // Enable the internal 3.3V pull-up on RX pin to match the PIC's open-drain 5V TX pin.
    nrf_gpio_cfg_input(g_ADigitalPinMap[PIN_SERIAL1_RX], NRF_GPIO_PIN_PULLUP);
  }

  void update() {
    // Drain the RX buffer and get the latest value
    bool received = false;
    while (Serial1.available() > 0) {
      uint8_t rx = Serial1.read();
      latest_dial_ = rx * (1.0f / 255);
      received = true;
    }

    if (!received) {
      return;  // Don't send if nothing received.
    }

    uint8_t tx = bypassed_ ? 0 : std::clamp<int>(target_power_ * 255, 1, 255);
    Serial1.write(tx);
  }

  float read() const override { return latest_dial_; }
  void write(float val) override { target_power_ = val; }
  void set(PinState state) override { bypassed_ = (state == PinState::High); }

private:
  const int rx_pin_;
  const int tx_pin_;
  float latest_dial_ = 0.0f;
  float target_power_ = 0.0f;
  bool bypassed_ = true;
};

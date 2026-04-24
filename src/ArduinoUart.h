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
    Serial1.begin(9600);

    // Enable the internal 3.3V pull-up on RX pin to match the PIC's open-drain 5V TX pin.
    nrf_gpio_cfg_input(g_ADigitalPinMap[rx_pin_], NRF_GPIO_PIN_PULLUP);
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

    uint8_t tx = passthrough_ ? 0 : std::clamp<int>(target_power_ * 255, 1, 255);
    Serial1.write(tx);
  }

  float read() const override { return latest_dial_; }

  void setPwm(float val) override { target_power_ = val; }
  void setPassthrough() override { passthrough_ = true; }

private:
  const int rx_pin_;
  const int tx_pin_;
  float latest_dial_ = 1.0f;
  float target_power_ = 0.0f;
  bool passthrough_ = true;
};

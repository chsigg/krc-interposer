#pragma once

#include "Buzzer.h"
#include <Arduino.h>
#include <cstdint>
#include <nrf_pwm.h>

class ArduinoBuzzer : public Buzzer {
  static constexpr uint16_t FREQUENCY_HZ = 4000;
  static constexpr uint16_t TOP = 16000000 / FREQUENCY_HZ;

public:
  ArduinoBuzzer(int pin_p, int pin_n) : pin_p_(pin_p), pin_n_(pin_n) {
    pinMode(pin_p_, OUTPUT);
    pinMode(pin_n_, OUTPUT);
    digitalWrite(pin_p_, LOW);
    digitalWrite(pin_n_, LOW);
  }

  void enable() override {
    NRF_PWM0->PSEL.OUT[0] = g_ADigitalPinMap[pin_p_];
    NRF_PWM0->PSEL.OUT[1] = g_ADigitalPinMap[pin_n_];

    // Configure PWM0 for differential drive
    NRF_PWM0->MODE = PWM_MODE_UPDOWN_Up;
    NRF_PWM0->PRESCALER = PWM_PRESCALER_PRESCALER_DIV_1; // 16MHz
    NRF_PWM0->COUNTERTOP = TOP;
    NRF_PWM0->LOOP = 0;
    NRF_PWM0->DECODER = (PWM_DECODER_LOAD_Individual << PWM_DECODER_LOAD_Pos) |
                        (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);

    // Restart sequence automatically
    NRF_PWM0->SHORTS = PWM_SHORTS_LOOPSDONE_SEQSTART0_Msk;

    // Normal and inverted polarity for channel 0 and 1
    uint16_t seq_[4] = {TOP / 2 | 0x0000, TOP / 2 | 0x8000, 0, 0};

    NRF_PWM0->SEQ[0].PTR = (uint32_t)seq_;
    NRF_PWM0->SEQ[0].CNT = 4;
    NRF_PWM0->SEQ[0].REFRESH = 0;
    NRF_PWM0->SEQ[0].ENDDELAY = 0;

    NRF_PWM0->ENABLE = 1;
    NRF_PWM0->TASKS_SEQSTART[0] = 1;
  }

  void disable() override {
    NRF_PWM0->TASKS_STOP = 1;
    NRF_PWM0->ENABLE = 0;
    NRF_PWM0->PSEL.OUT[0] = 0xFFFFFFFF;
    NRF_PWM0->PSEL.OUT[1] = 0xFFFFFFFF;
    digitalWrite(pin_p_, LOW);
    digitalWrite(pin_n_, LOW);
  }

private:
  const int pin_p_;
  const int pin_n_;
};

#pragma once

#include "Buzzer.h"
#include <Arduino.h>
#include <cstdint>
#include <nrf_pwm.h>

class ArduinoBuzzer : public Buzzer {

public:
  ArduinoBuzzer(int pin_p, int pin_n) : pin_p_(pin_p), pin_n_(pin_n) {
    pinMode(pin_p_, OUTPUT);
    pinMode(pin_n_, OUTPUT);
    digitalWrite(pin_p_, LOW);
    digitalWrite(pin_n_, LOW);
  }

  void enable(int32_t frequency_hz) override {
    int16_t period = 8000000 / 2 / frequency_hz;
    // Normal and inverted polarity for channel 0 and 1
    int16_t sequence[4] = {period, static_cast<int16_t>(period | 0x8000)};

    NRF_PWM3->PSEL.OUT[0] = g_ADigitalPinMap[pin_p_];
    NRF_PWM3->PSEL.OUT[1] = g_ADigitalPinMap[pin_n_];

    // Configure PWM3 for differential drive
    NRF_PWM3->MODE = PWM_MODE_UPDOWN_Up;
    NRF_PWM3->PRESCALER = PWM_PRESCALER_PRESCALER_DIV_2; // 8MHz
    NRF_PWM3->COUNTERTOP = period * 2;
    NRF_PWM3->LOOP = 0;
    NRF_PWM3->DECODER = (PWM_DECODER_LOAD_Individual << PWM_DECODER_LOAD_Pos) |
                        (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);

    // Restart sequence automatically
    NRF_PWM3->SHORTS = PWM_SHORTS_LOOPSDONE_SEQSTART0_Msk;

    NRF_PWM3->SEQ[0].PTR = (uint32_t)sequence;
    NRF_PWM3->SEQ[0].CNT = 4;
    NRF_PWM3->SEQ[0].REFRESH = 0;
    NRF_PWM3->SEQ[0].ENDDELAY = 0;

    NRF_PWM3->ENABLE = 1;
    NRF_PWM3->TASKS_SEQSTART[0] = 1;
  }

  void disable() override {
    NRF_PWM3->TASKS_STOP = 1;
    NRF_PWM3->ENABLE = 0;
    NRF_PWM3->PSEL.OUT[0] = 0xFFFFFFFF;
    NRF_PWM3->PSEL.OUT[1] = 0xFFFFFFFF;
    digitalWrite(pin_p_, LOW);
    digitalWrite(pin_n_, LOW);
  }

private:
  const int pin_p_;
  const int pin_n_;
};

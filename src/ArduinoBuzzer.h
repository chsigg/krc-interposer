#pragma once

#include "Buzzer.h"
#include <Arduino.h>
#include <cstdint>
#include <nrf_pwm.h>

class ArduinoBuzzer : public Buzzer {

public:
  ArduinoBuzzer(NRF_PWM_Type* pwm, int pin_p, int pin_n)
      : pwm_(pwm), pin_p_(pin_p), pin_n_(pin_n) {}

  virtual void begin() {
    pinMode(pin_p_, OUTPUT);
    pinMode(pin_n_, OUTPUT);
    digitalWrite(pin_p_, LOW);
    digitalWrite(pin_n_, LOW);
  }

  void enable(int32_t frequency_hz) override {
    int16_t period = 8000000 / 2 / frequency_hz;
    // Normal and inverted polarity for channel 0 and 1
    int16_t sequence[4] = {period, static_cast<int16_t>(period | 0x8000)};

    pwm_->PSEL.OUT[0] = g_ADigitalPinMap[pin_p_];
    pwm_->PSEL.OUT[1] = g_ADigitalPinMap[pin_n_];

    // Configure PWM3 for differential drive
    pwm_->MODE = PWM_MODE_UPDOWN_Up;
    pwm_->PRESCALER = PWM_PRESCALER_PRESCALER_DIV_2; // 8MHz
    pwm_->COUNTERTOP = period * 2;
    pwm_->LOOP = 0;
    pwm_->DECODER = (PWM_DECODER_LOAD_Individual << PWM_DECODER_LOAD_Pos) |
                        (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);

    // Restart sequence automatically
    pwm_->SHORTS = PWM_SHORTS_LOOPSDONE_SEQSTART0_Msk;

    pwm_->SEQ[0].PTR = (uint32_t)sequence;
    pwm_->SEQ[0].CNT = 4;
    pwm_->SEQ[0].REFRESH = 0;
    pwm_->SEQ[0].ENDDELAY = 0;

    pwm_->ENABLE = 1;
    pwm_->TASKS_SEQSTART[0] = 1;
  }

  void disable() override {
    pwm_->TASKS_STOP = 1;
    pwm_->ENABLE = 0;
    pwm_->PSEL.OUT[0] = 0xFFFFFFFF;
    pwm_->PSEL.OUT[1] = 0xFFFFFFFF;
    digitalWrite(pin_p_, LOW);
    digitalWrite(pin_n_, LOW);
  }

private:
  NRF_PWM_Type* pwm_;
  const int pin_p_;
  const int pin_n_;
};

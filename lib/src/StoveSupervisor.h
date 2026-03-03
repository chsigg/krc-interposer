#pragma once

#include "Beeper.h"
#include "StoveActuator.h"
#include "StoveDial.h"
#include "StoveThrottle.h"
#include "Thermometer.h"
#include "ThermalController.h"
#include "TrendAnalyzer.h"

struct StoveConfig {
  float min_temp_c = 30.0f;
  float max_temp_c = 120.0f;
};

class StoveSupervisor {
public:
  using PowerOffCallback = void (*)();

  StoveSupervisor(StoveDial &dial, StoveActuator &actuator,
                  ThermalController &controller, Beeper &beeper,
                  TrendAnalyzer &analyzer, Thermometer &thermometer,
                  DigitalWritePin &bypass_pin,
                  const StoveConfig &stove_config,
                  const ThrottleConfig &throttle_config,
                  PowerOffCallback power_off_cb);
  virtual ~StoveSupervisor() = default;

  void begin();
  void update();

private:
  enum class State {
    SCANNING,   // Waiting for connection
    CONNECTED,  // Connected, waiting 1s
    ACTIVE,     // PID control active
    DISCONNECTED // Signal lost
  };

  void transitionTo(State new_state);
  const char *getStateName(State state) const;

  StoveDial &dial_;
  StoveActuator &actuator_;
  ThermalController &controller_;
  Beeper &beeper_;
  TrendAnalyzer &analyzer_;
  Thermometer &thermometer_;
  DigitalWritePin &bypass_pin_;
  const StoveConfig stove_config_;
  const ThrottleConfig throttle_config_;
  PowerOffCallback power_off_cb_;

  State state_ = State::SCANNING;
  uint32_t state_entry_ms_ = 0;
  uint32_t dial_off_start_ms_ = 0;
  float dial_target_temp_ = -1.0f;
  bool is_temp_low_ = false;
};

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
  float base_power_ratio = 0.8f;
};

class StoveSupervisor {
public:
  StoveSupervisor(StoveDial &dial, StoveActuator &actuator,
                  ThermalController &controller, Beeper &beeper,
                  TrendAnalyzer &analyzer, Thermometer &thermometer,
                  const StoveConfig &stove_config,
                  const ThrottleConfig &throttle_config);
  virtual ~StoveSupervisor() = default;

  void update();

private:
  enum class State {
    SLEEP,      // Waiting for dial activity, BLE off
    SCANNING,   // Waiting for thermometer connection
    CONNECTED,  // Waiting for auto pos
    ACTIVATING, // Waiting for 3sec
    ACTIVE,     // PID control active
    DISCONNECTED, // Signal lost
    COOLDOWN    // Waiting for 30sec
  };

  void transitionTo(State new_state);
  const char *getStateName(State state) const;

  StoveThrottle pidToThrottle(float power) const;

  StoveDial &dial_;
  StoveActuator &actuator_;
  ThermalController &controller_;
  Beeper &beeper_;
  TrendAnalyzer &analyzer_;
  Thermometer &thermometer_;
  const StoveConfig stove_config_;
  const ThrottleConfig throttle_config_;

  State state_ = State::SLEEP;
  uint32_t state_entry_ms_ = 0;
  uint32_t dial_off_start_ms_ = 0;
  bool has_beeped_connected_ = false;
  float dial_target_temp_ = -1.0f;
};

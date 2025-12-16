#pragma once

#include "Beeper.h"
#include "StoveActuator.h"
#include "StoveDial.h"
#include "ThermalController.h"
#include "TrendAnalyzer.h"

struct StoveConfig {
  float min_temp_c = 20.0f;
  float max_temp_c = 120.0f;
  float base_power_ratio = 0.8f;
  uint32_t data_timeout_ms = 60 * 1000; // 1 minute
};

class StoveSupervisor {
public:
  StoveSupervisor(StoveDial &dial, StoveActuator &actuator,
                  ThermalController &controller, Beeper &beeper,
                  TrendAnalyzer &analyzer, const StoveConfig &config,
                  const ThrottleConfig &throttle_config);

  void update();

  // Call this when Thermometer connects or Shutter triggers
  void takeSnapshot();

private:
  StoveDial &dial_;
  StoveActuator &actuator_;
  ThermalController &controller_;
  Beeper &beeper_;
  TrendAnalyzer &analyzer_;
  const StoveConfig stove_config_;
  const ThrottleConfig throttle_config_;
  bool is_direct_mode_ = true;
  bool is_analyzer_timed_out_ = false;
};

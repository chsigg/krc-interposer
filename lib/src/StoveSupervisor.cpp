#include "StoveSupervisor.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

extern "C" uint32_t millis();

StoveSupervisor::StoveSupervisor(StoveDial &dial, StoveActuator &actuator,
                                 ThermalController &controller, Beeper &beeper,
                                 TrendAnalyzer &analyzer,
                                 const StoveConfig &stove_config,
                                 const ThrottleConfig &throttle_config)
    : dial_(dial), actuator_(actuator), controller_(controller),
      beeper_(beeper), analyzer_(analyzer), stove_config_(stove_config),
      throttle_config_(throttle_config) {}

void StoveSupervisor::takeSnapshot() {
  // Read current knob position (0.0 to 1.0)
  float knob_pos = dial_.getThrottle().base;

  // Map to Temp Range
  float target_temp =
      stove_config_.min_temp_c +
      (knob_pos * (stove_config_.max_temp_c - stove_config_.min_temp_c));

  controller_.setTargetTemp(target_temp);
  is_bypass_ = false;

  beeper_.beep(Beeper::Signal::ACCEPT);
}

void StoveSupervisor::update() {
  auto clear_timeout = [&] {
    if (!is_analyzer_timed_out_) {
      return;
    }
    Log << "StoveSupervisor::update() analyzer recovered\n";
    beeper_.beep(Beeper::Signal::NONE);
    is_analyzer_timed_out_ = false;
  };

  dial_.update();

  if (dial_.isOff()) {
    actuator_.setBypass();
    analyzer_.clear();
    clear_timeout();
    is_bypass_ = true;
  }

  if (is_bypass_) {
    return;
  }

  if (millis() - analyzer_.getLastUpdateMs() > stove_config_.data_timeout_ms) {
    if (!is_analyzer_timed_out_) {
      Log << "StoveSupervisor::update() analyzer timed out\n";
      is_analyzer_timed_out_ = true;
      actuator_.setThrottle(StoveThrottle{});
      beeper_.beep(Beeper::Signal::ERROR);
    }
    return;
  }

  if (analyzer_.getLastUpdateMs() == 0) {
    return;
  }

  clear_timeout();

  controller_.update();
  float pid_out = controller_.getLevel();

  StoveThrottle output = dial_.getThrottle();
  output.base = std::min(output.base, pid_out / stove_config_.base_power_ratio);

  float boost_level =
      std::max(0.0f, (pid_out - stove_config_.base_power_ratio) /
                         (1.0f - stove_config_.base_power_ratio));
  uint32_t pid_boost = std::ceil(boost_level * throttle_config_.num_boosts);
  output.boost = std::min(output.boost, pid_boost);

  actuator_.setThrottle(output);
}

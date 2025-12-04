#include "StoveSupervisor.h"
#include <algorithm>
#include <cmath>

extern "C" uint32_t millis();

StoveSupervisor::StoveSupervisor(StoveDial &dial, StoveActuator &actuator,
                                 ThermalController &controller, Beeper &beeper,
                                 const TrendAnalyzer &analyzer,
                                 const StoveConfig &stove_config,
                                 const LevelConfig &level_config)
    : dial_(dial), actuator_(actuator), controller_(controller),
      beeper_(beeper), analyzer_(analyzer), stove_config_(stove_config),
      level_config_(level_config) {}

void StoveSupervisor::takeSnapshot() {
  // Read current knob position (0.0 to 1.0)
  float knob_pos = dial_.getLevel().base;

  // Map to Temp Range
  float target_temp = stove_config_.min_temp_c +
                      (knob_pos * (stove_config_.max_temp_c - stove_config_.min_temp_c));

  controller_.setTargetTemp(target_temp);

  beeper_.beep(Beeper::Signal::ACCEPT);
}

void StoveSupervisor::update() {
  uint32_t now = millis();

  if (analyzer_.getLastUpdateMs() > 0 &&
      now - analyzer_.getLastUpdateMs() > stove_config_.data_timeout_ms) {
    actuator_.setLevel(StoveLevel{});
    // Alarm beep every second
    if (now % 1000 < 100) {
      beeper_.beep(Beeper::Signal::ERROR);
    }
    return;
  }

  float pid_out = controller_.getLevel();

  StoveLevel output = dial_.getLevel();
  output.base = std::min(output.base, pid_out / stove_config_.base_power_ratio);

  float boost_level = std::max(0.0f, (pid_out - stove_config_.base_power_ratio) /
                                         (1.0f - stove_config_.base_power_ratio));
  uint32_t pid_boost = std::ceil(boost_level * level_config_.num_boosts);
  output.boost = std::clamp(pid_boost, output.boost, level_config_.num_boosts);

  actuator_.setLevel(output);
}

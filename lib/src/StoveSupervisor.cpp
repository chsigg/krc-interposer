#include "StoveSupervisor.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

extern "C" uint32_t millis();

StoveSupervisor::StoveSupervisor(StoveDial &dial, StoveActuator &actuator,
                                 ThermalController &controller, Beeper &beeper,
                                 TrendAnalyzer &analyzer,
                                 Thermometer &thermometer,
                                 const StoveConfig &stove_config,
                                 const ThrottleConfig &throttle_config)
    : dial_(dial), actuator_(actuator), controller_(controller),
      beeper_(beeper), analyzer_(analyzer), thermometer_(thermometer),
      stove_config_(stove_config), throttle_config_(throttle_config) {}

void StoveSupervisor::update() {
  uint32_t now = millis();
  dial_.update();
  beeper_.update();

  switch (state_) {
  case State::SLEEP:
    if (!dial_.isOff()) {
      return transitionTo(State::SCANNING);
    }
    break;
  case State::SCANNING:
    if (dial_.isOff()) {
      return transitionTo(State::COOLDOWN);
    }
    if (thermometer_.connected()) {
      return transitionTo(State::CONNECTED);
    }
    break;
  case State::CONNECTED:
    if (dial_.isOff()) {
      return transitionTo(State::COOLDOWN);
    }
    if (!thermometer_.connected()) {
      return transitionTo(State::SCANNING);
    }
    if (dial_.isAutoPosition()) {
      return transitionTo(State::ACTIVATING);
    }
    break;
  case State::ACTIVATING:
    if (dial_.isOff()) {
      return transitionTo(State::COOLDOWN);
    }
    if (now - state_entry_ms_ > 3000) {
      return transitionTo(State::ACTIVE);
    }
    break;
  case State::ACTIVE:
    if (dial_.isOff()) {
      return transitionTo(State::COOLDOWN);
    }
    if (now - state_entry_ms_ >= 300) {
      float pos = dial_.getPosition();
      float target =
          stove_config_.min_temp_c +
          pos * (stove_config_.max_temp_c - stove_config_.min_temp_c);

      controller_.setTargetTemp(target);
      controller_.update();
      float pid = controller_.getLevel();
      actuator_.setThrottle(pidToThrottle(pid));
    }
    break;
  case State::COOLDOWN:
    if (!dial_.isOff()) {
      return transitionTo(State::SCANNING);
    }
    if (now - state_entry_ms_ > stove_config_.data_timeout_ms) {
      return transitionTo(State::SLEEP);
    }
    break;
  }
}

void StoveSupervisor::transitionTo(State new_state) {
  if (state_ == new_state) {
    return;
  }

  Log << "StoveSupervisor: " << getStateName(state_) << " -> "
      << getStateName(new_state) << "\n";

  state_ = new_state;
  state_entry_ms_ = millis();

  if (state_ == State::ACTIVE) {
    actuator_.setThrottle({0.0f, 0}); // Disables bypass
  } else {
    actuator_.setBypass();
  }

  if (state_ == State::SCANNING) {
    thermometer_.start();
  }

  if (state_ == State::SLEEP) {
    thermometer_.stop();
    has_beeped_connected_ = false;
  }

  if (state_ == State::CONNECTED) {
    if (!has_beeped_connected_) {
      beeper_.beep(Beeper::Signal::ACCEPT);
    }
    has_beeped_connected_ = true;
  }
}

const char *StoveSupervisor::getStateName(State state) const {
  switch (state) {
  case State::SLEEP:
    return "SLEEP";
  case State::SCANNING:
    return "SCANNING";
  case State::CONNECTED:
    return "CONNECTED";
  case State::ACTIVATING:
    return "ACTIVATING";
  case State::ACTIVE:
    return "ACTIVE";
  case State::COOLDOWN:
    return "COOLDOWN";
  }
  return "UNKNOWN";
}

StoveThrottle StoveSupervisor::pidToThrottle(float pid_level) const {
  StoveThrottle output = {};

  if (pid_level <= stove_config_.base_power_ratio) {
    output.base = pid_level / stove_config_.base_power_ratio;
    output.boost = 0;
  } else {
    output.base = 1.0f;
    float boost_range = 1.0f - stove_config_.base_power_ratio;
    float boost_val =
        (pid_level - stove_config_.base_power_ratio) / boost_range;
    output.boost = std::ceil(boost_val * throttle_config_.num_boosts);
  }

  return output;
}

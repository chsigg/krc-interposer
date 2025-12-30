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

static float lerp(float a, float b, float t) { return a + t * (b - a); }

void StoveSupervisor::update() {
  uint32_t now = millis();
  dial_.update();
  beeper_.update();

  if (!dial_.isOff()) {
    dial_off_start_ms_ = now;
  }

  constexpr uint32_t cooldown_after_ms = 1000;
  constexpr uint32_t active_after_ms = 3 * 1000;
  constexpr uint32_t stove_clear_duration_ms = 300;
  constexpr uint32_t disconnected_after_ms = 30 * 1000;
  constexpr uint32_t sleep_after_ms = 10 * 1000;

  if (state_ == State::SLEEP || state_ == State::COOLDOWN) {
    if (!dial_.isOff()) {
      return transitionTo(State::SCANNING);
    }
  } else {
    if (dial_.isOff() && now - dial_off_start_ms_ > cooldown_after_ms) {
      return transitionTo(State::COOLDOWN);
    }
  }

  switch (state_) {
  case State::SLEEP:
    break;
  case State::SCANNING:
    if (thermometer_.connected()) {
      return transitionTo(State::CONNECTED);
    }
    break;
  case State::CONNECTED:
    if (!thermometer_.connected()) {
      return transitionTo(State::SCANNING);
    }
    if (dial_.isBoil()) {
      return transitionTo(State::ACTIVATING);
    }
    break;
  case State::ACTIVATING:
    if (now - state_entry_ms_ > active_after_ms) {
      return transitionTo(State::ACTIVE);
    }
    break;
  case State::ACTIVE:
    if (now - state_entry_ms_ < stove_clear_duration_ms) {
      return;
    }
    if (now - analyzer_.getLastUpdateMs() > disconnected_after_ms) {
      return transitionTo(State::DISCONNECTED);
    }
    if (float dial_target_temp =
            lerp(stove_config_.min_temp_c, stove_config_.max_temp_c,
                 dial_.getPosition());
        std::abs(dial_target_temp - dial_target_temp_) > 0.02f) {
      controller_.setTargetTemp(dial_target_temp);
      dial_target_temp_ = dial_target_temp;
    }
    controller_.update();
    actuator_.setThrottle(pidToThrottle(controller_.getPower()));
    break;
  case State::DISCONNECTED:
    if (now - analyzer_.getLastUpdateMs() < disconnected_after_ms) {
      return transitionTo(State::ACTIVE);
    }
    break;
  case State::COOLDOWN:
    if (now - state_entry_ms_ > sleep_after_ms) {
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

  if (state_ != State::ACTIVE && state_ != State::DISCONNECTED) {
    actuator_.setBypass();
  }

  switch (state_) {
  case State::SLEEP:
    thermometer_.stop();
    beeper_.beep(Beeper::Signal::REJECT);
    has_beeped_connected_ = false;
    break;
  case State::SCANNING:
    thermometer_.start();
    break;
  case State::CONNECTED:
    if (!has_beeped_connected_) {
      beeper_.beep(Beeper::Signal::ACCEPT);
    }
    has_beeped_connected_ = true;
    break;
  case State::ACTIVATING:
    break;
  case State::ACTIVE:
    actuator_.setThrottle({std::max(0.0f, dial_.getPosition() - 0.1f), 0});
    dial_target_temp_ = -1.0f;
    beeper_.beep(Beeper::Signal::NONE);
    break;
  case State::DISCONNECTED:
    actuator_.setThrottle({0.0f, 0});
    beeper_.beep(Beeper::Signal::ERROR);
    break;
  case State::COOLDOWN:
    beeper_.beep(Beeper::Signal::NONE);
    break;
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
  case State::DISCONNECTED:
    return "DISCONNECTED";
  case State::COOLDOWN:
    return "COOLDOWN";
  }
  return "UNKNOWN";
}

StoveThrottle StoveSupervisor::pidToThrottle(float power) const {
  if (power <= stove_config_.base_power_ratio) {
    return {power / stove_config_.base_power_ratio};
  }

  float boost_range = 1.0f - stove_config_.base_power_ratio;
  float boost_level = (power - stove_config_.base_power_ratio) / boost_range;
  long boost_value = std::lroundf(boost_level * throttle_config_.num_boosts);
  return {1.0f, static_cast<uint32_t>(boost_value)};
}

#include "StoveSupervisor.h"
#include "Logger.h"
#include "StoveThrottle.h"
#include <algorithm>
#include <cmath>

extern "C" uint32_t millis();

StoveSupervisor::StoveSupervisor(StoveDial &dial, StoveController &controller,
                                 ThermalController &thermal_controller, Beeper &beeper,
                                 TrendAnalyzer &analyzer,
                                 const StoveConfig &stove_config,
                                 const ThrottleConfig &throttle_config,
                                 PowerOffCallback power_off_cb)
    : dial_(dial), stove_controller_(controller), thermal_controller_(thermal_controller),
      beeper_(beeper), analyzer_(analyzer),
      stove_config_(stove_config), throttle_config_(throttle_config),
      power_off_cb_(power_off_cb) {}

static float lerp(float a, float b, float t) { return a + t * (b - a); }

void StoveSupervisor::update() {
  dial_.update();
  beeper_.update();
  stove_controller_.update();

  if (float dial_target_temp =
          lerp(stove_config_.min_temp_c, stove_config_.max_temp_c,
               dial_.getPosition());
      std::fabs(dial_target_temp - dial_target_temp_) > 1.0f) {
    thermal_controller_.setTargetTemp(dial_target_temp);
    dial_target_temp_ = dial_target_temp;
  }

  uint32_t now_ms = millis();
  if (!dial_.isOff()) {
    dial_off_start_ms_ = now_ms;
  }
  if (now_ms - dial_off_start_ms_ > 5 * 1000 && power_off_cb_) {
    return power_off_cb_();
  }

  constexpr uint32_t connected_wait_ms = 1000;

  switch (state_) {
  case State::SCANNING:
    stove_controller_.setThrottle({dial_.getPosition(), 0});
    if (analyzer_.connected()) {
      return transitionTo(State::CONNECTED);
    }
    break;
  case State::CONNECTED:
    if (!analyzer_.connected()) {
      return transitionTo(State::DISCONNECTED);
    }
    if (now_ms - state_entry_ms_ > connected_wait_ms) {
      return transitionTo(State::ACTIVE);
    }
    stove_controller_.setThrottle(
        {lerp(1.0f, throttle_config_.min,
              static_cast<float>(now_ms - state_entry_ms_) / connected_wait_ms),
         0});
    stove_controller_.setPassthrough();
    break;
  case State::ACTIVE:
    if (!analyzer_.connected()) {
      return transitionTo(State::DISCONNECTED);
    }
    if (dial_.isOff()) {
      stove_controller_.setMinThrottle();
      break;
    }
    thermal_controller_.update();

    // Engage boost if we are > 20°C away (approx 60s of heating).
    // Disengage boost if we are < 10°C away and hand over to PID.
    is_temp_low_ = [&] {
      float delta_temp =
          thermal_controller_.getTargetTemp() - analyzer_.getValue(now_ms);
      return delta_temp >= (is_temp_low_ ? 10.0f : 20.0f);
    }();

    stove_controller_.setThrottle({is_temp_low_ ? 1.0f : thermal_controller_.getPower(),
                           is_temp_low_ ? throttle_config_.num_boosts : 0});
    stove_controller_.setPassthrough();
    break;
  case State::DISCONNECTED:
    if (analyzer_.connected()) {
      return transitionTo(State::ACTIVE);
    }
    stove_controller_.setMinThrottle();
    stove_controller_.setPassthrough();
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

  switch (state_) {
  case State::SCANNING:
    break;
  case State::CONNECTED:
    beeper_.beep(Beeper::Signal::CONNECTED);
    break;
  case State::ACTIVE:
    beeper_.beep(Beeper::Signal::NONE);
    break;
  case State::DISCONNECTED:
    beeper_.beep(Beeper::Signal::DISCONNECTED);
    break;
  }
}

const char *StoveSupervisor::getStateName(State state) const {
  switch (state) {
  case State::SCANNING:
    return "SCANNING";
  case State::CONNECTED:
    return "CONNECTED";
  case State::ACTIVE:
    return "ACTIVE";
  case State::DISCONNECTED:
    return "DISCONNECTED";
  }
  return "UNKNOWN";
}

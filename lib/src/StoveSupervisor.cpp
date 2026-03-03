#include "StoveSupervisor.h"
#include "Logger.h"
#include "StoveThrottle.h"
#include <algorithm>
#include <cmath>

extern "C" uint32_t millis();

StoveSupervisor::StoveSupervisor(StoveDial &dial, StoveActuator &actuator,
                                 ThermalController &controller, Beeper &beeper,
                                 TrendAnalyzer &analyzer,
                                 Thermometer &thermometer,
                                 DigitalWritePin &bypass_pin,
                                 const StoveConfig &stove_config,
                                 const ThrottleConfig &throttle_config,
                                 PowerOffCallback power_off_cb)
    : dial_(dial), actuator_(actuator), controller_(controller),
      beeper_(beeper), analyzer_(analyzer), thermometer_(thermometer),
      bypass_pin_(bypass_pin),
      stove_config_(stove_config), throttle_config_(throttle_config),
      power_off_cb_(power_off_cb) {}

static float lerp(float a, float b, float t) { return a + t * (b - a); }

void StoveSupervisor::begin() {
}

void StoveSupervisor::update() {
  dial_.update();
  beeper_.update();
  thermometer_.update();

  uint32_t now = millis();
  if (!dial_.isOff()) {
    dial_off_start_ms_ = now;
  }
  if (now - dial_off_start_ms_ > 5000 && power_off_cb_) {
    return power_off_cb_();
  }

  if (!thermometer_.connected()) {
    return transitionTo(State::SCANNING);
  }

  constexpr uint32_t connected_wait_ms = 1000;
  constexpr uint32_t disconnected_after_ms = 30 * 1000;

  switch (state_) {
  case State::SCANNING:
    if (thermometer_.connected()) {
      return transitionTo(State::CONNECTED);
    }
    break;
  case State::CONNECTED:
    if (now - state_entry_ms_ > connected_wait_ms) {
      return transitionTo(State::ACTIVE);
    }
    break;
  case State::ACTIVE:
    if (now - analyzer_.getLastUpdateMs() > disconnected_after_ms) {
      return transitionTo(State::DISCONNECTED);
    }
    if (float dial_target_temp =
            lerp(stove_config_.min_temp_c, stove_config_.max_temp_c,
                 dial_.getPosition());
        std::abs(dial_target_temp - dial_target_temp_) > 1.0f) {
      controller_.setTargetTemp(dial_target_temp);
      dial_target_temp_ = dial_target_temp;
    }
    controller_.update();

    // Engage boost if we are > 20°C away (approx 60s of heating).
    // Disengage boost if we are < 10°C away and hand over to PID.
    is_temp_low_ = [&] {
      if (controller_.isLidOpen()) {
        return false;
      }
      float delta_temp = controller_.getTargetTemp() - analyzer_.getValue(now);
      return delta_temp >= (is_temp_low_ ? 10.0f : 20.0f);
    }();

    actuator_.setThrottle({is_temp_low_ ? 1.0f : controller_.getPower(),
                           is_temp_low_ ? throttle_config_.num_boosts : 0});
    break;
  case State::DISCONNECTED:
    if (now - analyzer_.getLastUpdateMs() < disconnected_after_ms) {
      return transitionTo(State::ACTIVE);
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
  bypass_pin_.set(state_ == State::SCANNING ? PinState::Low : PinState::High);

  switch (state_) {
  case State::SCANNING:
    break;
  case State::CONNECTED:
    beeper_.beep(Beeper::Signal::ACCEPT);
    actuator_.setThrottle({0.0f, 0});
    break;
  case State::ACTIVE:
    dial_target_temp_ = -1.0f;
    beeper_.beep(Beeper::Signal::NONE);
    break;
  case State::DISCONNECTED:
    actuator_.setThrottle({0.0f, 0});
    beeper_.beep(Beeper::Signal::ERROR);
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

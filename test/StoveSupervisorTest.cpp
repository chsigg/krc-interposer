#include <doctest.h>
#include <ArduinoFake.h>

#include "StoveSupervisor.h"
#include "StoveDial.h"
#include "StoveActuator.h"
#include "ThermalController.h"
#include "Beeper.h"
#include "TrendAnalyzer.h"
#include "Thermometer.h"
#include <cmath>

using namespace fakeit;

TEST_CASE("StoveSupervisor Logic") {
  // Global Mocks
  Fake(Method(ArduinoFake(), delayMicroseconds));

  // --- Configs ---
  ThrottleConfig throttle_config;
  StoveConfig stove_config;

  // --- Collaborator Mocks ---
  Mock<StoveDial> dial_mock;
  Mock<StoveActuator> actuator_mock;
  Mock<Beeper> beeper_mock;
  Mock<TrendAnalyzer> analyzer_mock;
  Mock<ThermalController> controller_mock;
  Mock<Thermometer> thermometer_mock;
  
  static bool poweroff_called = false;
  poweroff_called = false;
  auto poweroff_fn = []() { poweroff_called = true; };

  // --- DUT ---
  StoveSupervisor supervisor(dial_mock.get(), actuator_mock.get(),
                             controller_mock.get(), beeper_mock.get(),
                             analyzer_mock.get(), thermometer_mock.get(),
                             stove_config, throttle_config, poweroff_fn);

  uint32_t current_time_ms = 0;
  When(Method(ArduinoFake(), millis)).AlwaysDo([&]() { return current_time_ms; });
  auto set_time = [&](uint32_t t) {
    current_time_ms = t;
  };
  set_time(0);

  auto isNear = [](const StoveThrottle &t, const StoveThrottle &expected) {
    return std::fabs(t.position - expected.position) < 0.001f &&
           t.boost == expected.boost;
  };

  // --- Common Stubs ---
  When(Method(dial_mock, getPosition)).AlwaysReturn(0.0f);
  When(Method(dial_mock, isOff)).AlwaysReturn(false);
  When(Method(dial_mock, isBoil)).AlwaysReturn(false);
  Fake(Method(dial_mock, update));
  Fake(Method(actuator_mock, setThrottle));
  Fake(Method(actuator_mock, update));
  Fake(Method(beeper_mock, beep));
  Fake(Method(beeper_mock, update));
  When(Method(controller_mock, getPower)).AlwaysReturn(0.0f);
  Fake(Method(controller_mock, setTargetTemp));
  Fake(Method(controller_mock, update));
  When(Method(controller_mock, getTargetTemp)).AlwaysReturn(0.0f);
  When(Method(controller_mock, isLidOpen)).AlwaysReturn(false);
  When(Method(thermometer_mock, connected)).AlwaysReturn(false);
  Fake(Method(thermometer_mock, update));
  When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(0);
  When(Method(analyzer_mock, getValue)).AlwaysReturn(0.0f);

  auto reset_actuator = [&]() {
    actuator_mock.Reset();
    Fake(Method(actuator_mock, setThrottle));
    Fake(Method(actuator_mock, update));
  };

  SUBCASE("Initial state is SCANNING") {
    When(Method(dial_mock, isOff)).AlwaysReturn(true);
    supervisor.update();
    CHECK(poweroff_called == false);
  }

  SUBCASE("Power off after 5s dial off") {
    When(Method(dial_mock, isOff)).AlwaysReturn(true);
    set_time(5001);
    supervisor.update();
    CHECK(poweroff_called == true);
  }

  SUBCASE("SCANNING behavior") {
    SUBCASE("Transition SCANNING -> CONNECTED") {
      When(Method(thermometer_mock, connected)).AlwaysReturn(true);
      supervisor.update();
      // CONNECTED entry sets throttle to 0 and beeps ACCEPT
      Verify(Method(beeper_mock, beep).Using(Beeper::Signal::ACCEPT)).Once();
      Verify(Method(actuator_mock, setThrottle).Matching([&](const StoveThrottle &t) {
        return isNear(t, StoveThrottle{0.0f, 0});
      })).Once();
    }
  }

  SUBCASE("CONNECTED behavior") {
    // Get to CONNECTED
    When(Method(thermometer_mock, connected)).AlwaysReturn(true);
    supervisor.update(); // SCANNING -> CONNECTED
    reset_actuator();

    SUBCASE("Transition CONNECTED -> ACTIVE after 1s") {
      set_time(1001);
      supervisor.update();
      Verify(Method(beeper_mock, beep).Using(Beeper::Signal::NONE)).Once();
    }

    SUBCASE("Transition CONNECTED -> SCANNING on disconnect") {
      When(Method(thermometer_mock, connected)).AlwaysReturn(false);
      supervisor.update();
    }
  }

  SUBCASE("ACTIVE behavior") {
    // Fast forward to ACTIVE
    When(Method(thermometer_mock, connected)).AlwaysReturn(true);
    supervisor.update(); // SCANNING -> CONNECTED
    set_time(1001);
    supervisor.update(); // CONNECTED -> ACTIVE

    reset_actuator();

    SUBCASE("PID Control Loop") {
      When(Method(dial_mock, getPosition)).AlwaysReturn(0.5f);
      When(Method(controller_mock, getPower)).AlwaysReturn(0.4f);
      When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(1001);

      supervisor.update();

      Verify(Method(controller_mock, setTargetTemp)).Once();
      Verify(Method(controller_mock, update)).Once();
      Verify(Method(actuator_mock, setThrottle)).Once();
    }

    SUBCASE("Boost logic") {
      When(Method(dial_mock, getPosition)).AlwaysReturn(1.0f);
      When(Method(controller_mock, getTargetTemp)).AlwaysReturn(120.0f);
      When(Method(controller_mock, isLidOpen)).AlwaysReturn(false);
      When(Method(controller_mock, getPower)).AlwaysReturn(0.123f);
      When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(1001);

      auto verify_throttle = [&](float expected_pos, uint32_t expected_boost) {
        Verify(Method(actuator_mock, setThrottle).Matching([&](const StoveThrottle &t) {
          return isNear(t, StoveThrottle{expected_pos, expected_boost});
        })).Once();
      };

      SUBCASE("Engages boost when > 20 degrees away") {
        When(Method(analyzer_mock, getValue)).AlwaysReturn(90.0f); // Delta 30
        supervisor.update();
        verify_throttle(1.0f, throttle_config.num_boosts);
      }

      SUBCASE("Disengages boost when < 10 degrees away") {
        // First pass: Enable boost
        When(Method(analyzer_mock, getValue)).AlwaysReturn(90.0f);
        supervisor.update();
        reset_actuator();

        // Second pass: Delta 5 (120 - 115) -> Disable boost
        When(Method(analyzer_mock, getValue)).AlwaysReturn(115.0f);
        supervisor.update();
        verify_throttle(0.123f, 0);
      }
    }

    SUBCASE("Transition ACTIVE -> DISCONNECTED on signal loss") {
      set_time(1001 + 30001);
      When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(1001);

      supervisor.update();

      Verify(Method(actuator_mock, setThrottle).Matching([&](const StoveThrottle &t) {
        return isNear(t, StoveThrottle{0.0f, 0});
      })).Once();
      Verify(Method(beeper_mock, beep).Using(Beeper::Signal::ERROR)).Once();

      SUBCASE("Transition DISCONNECTED -> ACTIVE on signal recovery") {
        When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(current_time_ms);
        supervisor.update();
        // ACTIVE entry resets target temp
      }
    }
  }
}

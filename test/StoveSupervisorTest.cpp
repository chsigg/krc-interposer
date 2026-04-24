#include <ArduinoFake.h>
#include <doctest.h>
#include <cmath>

#include "Beeper.h"
#include "StoveController.h"
#include "StoveDial.h"
#include "StoveSupervisor.h"
#include "ThermalController.h"
#include "TrendAnalyzer.h"

using namespace fakeit;

TEST_CASE("StoveSupervisor Logic") {
  // Global Mocks
  Fake(Method(ArduinoFake(), delayMicroseconds));

  // --- Configs ---
  ThrottleConfig throttle_config;
  StoveConfig stove_config;

  // --- Collaborator Mocks ---
  Mock<StoveDial> dial_mock;
  Mock<StoveController> controller_mock;
  Mock<Beeper> beeper_mock;
  Mock<TrendAnalyzer> analyzer_mock;
  Mock<ThermalController> thermal_controller_mock;

  static bool poweroff_called = false;
  poweroff_called = false;
  auto poweroff_fn = []() { poweroff_called = true; };

  // --- DUT ---
  StoveSupervisor supervisor(dial_mock.get(), controller_mock.get(),
                             thermal_controller_mock.get(), beeper_mock.get(),
                             analyzer_mock.get(),
                             stove_config, throttle_config);

  static uint32_t current_time_ms = 0;
  current_time_ms = 0;
  When(Method(ArduinoFake(), millis)).AlwaysDo([]() {
    return current_time_ms;
  });
  auto set_time = [](uint32_t t) { current_time_ms = t; };
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
  Fake(Method(controller_mock, setThrottle));
  Fake(Method(controller_mock, setPassthrough));
  Fake(Method(controller_mock, update));
  Fake(Method(beeper_mock, beep));
  Fake(Method(beeper_mock, update));
  When(Method(beeper_mock, isIdle)).AlwaysReturn(false);
  When(Method(thermal_controller_mock, getPower)).AlwaysReturn(0.0f);
  Fake(Method(thermal_controller_mock, setTargetTemp));
  Fake(Method(thermal_controller_mock, update));
  When(Method(thermal_controller_mock, getTargetTemp)).AlwaysReturn(0.0f);
  When(Method(analyzer_mock, connected)).AlwaysReturn(false);
  When(Method(analyzer_mock, getValue)).AlwaysReturn(0.0f);

  SUBCASE("Power off sequence") {
    When(Method(dial_mock, isOff)).AlwaysReturn(true);
    supervisor.update();

    set_time(5001);
    supervisor.update();
    CHECK(poweroff_called == true);
  }

  SUBCASE("SCANNING behavior") {
    SUBCASE("Transition SCANNING -> CONNECTED") {
      When(Method(analyzer_mock, connected)).AlwaysReturn(true);
      supervisor.update(); // transition happens here
      // CONNECTED entry beeps CONNECTED
      Verify(Method(beeper_mock, beep).Using(Beeper::Signal::CONNECTED)).Once();

      supervisor.update(); // first run of CONNECTED state
      // First update at t=0 should ramp starting at 1.0f
      Verify(Method(controller_mock, setThrottle)
                 .Matching([&](const StoveThrottle &t) {
                   return isNear(t, StoveThrottle{1.0f, 0});
                 }))
          .Once();
    }
  }

  SUBCASE("CONNECTED behavior") {
    // Get to CONNECTED
    When(Method(analyzer_mock, connected)).AlwaysReturn(true);
    supervisor.update(); // SCANNING -> CONNECTED
    controller_mock.ClearInvocationHistory();

    SUBCASE("Ramp down during CONNECTED") {
      set_time(500);
      supervisor.update();
      Verify(Method(controller_mock, setThrottle)
                 .Matching([&](const StoveThrottle &t) {
                   return isNear(t, StoveThrottle{0.575f, 0});
                 }))
          .Once();
    }

    SUBCASE("Transition CONNECTED -> ACTIVE after 1s") {
      set_time(1001);
      supervisor.update();
      Verify(Method(beeper_mock, beep).Using(Beeper::Signal::NONE)).Once();
    }

    SUBCASE("Transition CONNECTED -> DISCONNECTED on disconnect") {
      When(Method(analyzer_mock, connected)).AlwaysReturn(false);
      supervisor.update();
      Verify(Method(beeper_mock, beep).Using(Beeper::Signal::DISCONNECTED))
          .Once();
    }
  }

  SUBCASE("ACTIVE behavior") {
    // Fast forward to ACTIVE
    When(Method(analyzer_mock, connected)).AlwaysReturn(true);
    supervisor.update(); // SCANNING -> CONNECTED
    set_time(1001);
    supervisor.update(); // CONNECTED -> ACTIVE

    controller_mock.ClearInvocationHistory();

    SUBCASE("PID Control Loop") {
      When(Method(dial_mock, getPosition)).AlwaysReturn(0.5f);
      When(Method(thermal_controller_mock, getPower)).AlwaysReturn(0.4f);

      thermal_controller_mock.ClearInvocationHistory();
      supervisor.update();

      Verify(Method(controller_mock, setPassthrough)).Once();
      Verify(Method(thermal_controller_mock, setTargetTemp)).Once();
      Verify(Method(thermal_controller_mock, update)).Once();
      Verify(Method(controller_mock, setThrottle)).Once();
    }



    SUBCASE("Transition ACTIVE -> DISCONNECTED on signal loss") {
      // In the new architecture, the timeout is handled by the analyzer's
      // source (BleThermometer). So we simulate the analyzer reporting
      // disconnected.
      When(Method(analyzer_mock, connected)).AlwaysReturn(false);

      beeper_mock.ClearInvocationHistory();

      supervisor.update(); // transition
      Verify(Method(beeper_mock, beep).Using(Beeper::Signal::DISCONNECTED))
          .Once();

      controller_mock.ClearInvocationHistory();
      supervisor.update(); // first run of DISCONNECTED state
      Verify(Method(controller_mock, setThrottle).Matching([&](const StoveThrottle &t) { return t.position == throttle_config.min && t.boost == 0; })).AtLeast(1);
    }
  }
}

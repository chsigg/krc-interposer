#include <doctest.h>
#include <ArduinoFake.h>
#include <vector>

#include "StoveSupervisor.h"
#include "StoveDial.h"
#include "StoveActuator.h"
#include "ThermalController.h"
#include "Beeper.h"
#include "Potentiometer.h"
#include "TrendAnalyzer.h"
#include "Logger.h"

// Interfaces
#include "AnalogReadPin.h"
#include "DigitalWritePin.h"
#include "Buzzer.h"

using namespace fakeit;

TEST_CASE("StoveSupervisor Logic") {
  // Global Mocks
  Fake(Method(ArduinoFake(), millis));
  Fake(Method(ArduinoFake(), delayMicroseconds));

  // --- Configs ---
  ThrottleConfig throttle_config{.num_boosts = 2};
  StoveConfig stove_config;

  // --- Collaborator Mocks ---
  Mock<StoveDial> dial_mock;
  Mock<StoveActuator> actuator_mock;
  Mock<Beeper> beeper_mock;
  Mock<TrendAnalyzer> analyzer_mock;
  Mock<ThermalController> controller_mock;

  // --- DUT ---
  StoveSupervisor supervisor(dial_mock.get(), actuator_mock.get(),
                             controller_mock.get(), beeper_mock.get(),
                             analyzer_mock.get(), stove_config, throttle_config);

  auto set_time = [](uint32_t t) {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(t);
  };
  set_time(0);

  // --- Common Stubs ---
  When(Method(dial_mock, getThrottle)).AlwaysReturn(StoveThrottle{});
  When(Method(dial_mock, getPosition)).AlwaysReturn(0.0f);
  When(Method(dial_mock, isOff)).AlwaysReturn(false);
  Fake(Method(dial_mock, update));
  Fake(Method(actuator_mock, setBypass));
  Fake(Method(actuator_mock, setThrottle));
  Fake(Method(actuator_mock, update));
  Fake(Method(beeper_mock, beep));
  When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(0);
  Fake(Method(analyzer_mock, clear));
  When(Method(controller_mock, getLevel)).AlwaysReturn(0.0f);
  Fake(Method(controller_mock, setTargetTemp));
  Fake(Method(controller_mock, update));


  SUBCASE("Starts in bypass") {
    When(Method(dial_mock, getPosition)).Return(0.7f);
    supervisor.update();
    Verify(Method(actuator_mock, setBypass)).Never();
    Verify(Method(actuator_mock, setThrottle)).Never();
  }

  SUBCASE("Snapshot exits bypass and updates target temp") {
    // 1. Setup Dial to return 50%
    StoveThrottle throttle_50 = {0.5f, 0};
    When(Method(dial_mock, getThrottle)).AlwaysReturn(throttle_50);

    // 2. Take snapshot
    supervisor.takeSnapshot();

    // Verify Temp: 20 + 0.5 * (120 - 20) = 70.0
    Verify(Method(controller_mock, setTargetTemp).Using(70.0f)).Once();
    Verify(Method(beeper_mock, beep).Using(Beeper::Signal::ACCEPT)).Once();

    // 3. Verify it's now in PID mode
    When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(100);
    set_time(100);
    supervisor.update();
    Verify(Method(actuator_mock, setThrottle)).Once();
    // Should not call setPosition because it's no longer in bypass.
    // The call during Dial Off check is a separate test.
    Verify(Method(actuator_mock, setBypass)).Never();
  }

  SUBCASE("Dial Off resets to bypass") {
    // 1. Take snapshot to exit bypass
    supervisor.takeSnapshot();

    // 2. Set dial to off
    When(Method(dial_mock, isOff)).Return(true);
    When(Method(dial_mock, getPosition)).Return(0.0f);
    supervisor.update();

    // 3. Verify it cleared analyzer and went back to bypass
    Verify(Method(analyzer_mock, clear)).Once();
    Verify(Method(actuator_mock, setBypass)).Once();
  }

  SUBCASE("Safety check for stale data") {
    // Capture arguments to avoid reference-to-stack issues with FakeIt
    std::vector<StoveThrottle> throttles;
    When(Method(actuator_mock, setThrottle)).AlwaysDo([&](const StoveThrottle &t) {
      throttles.push_back(t);
    });

    // 1. Exit bypass and establish a healthy state
    supervisor.takeSnapshot();
    Verify(Method(beeper_mock, beep).Using(Beeper::Signal::ACCEPT)).Once();
    set_time(1000);
    When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(1000);
    supervisor.update();
    CHECK(throttles.size() == 1);

    // 2. Setup Stale Data condition
    set_time(1000 + stove_config.data_timeout_ms + 1);

    // 3. Expect Safety Shutdown
    supervisor.update();
    Verify(Method(actuator_mock, setBypass)).Never();
    CHECK(throttles.size() == 2);
    CHECK(isNear(throttles[1], StoveThrottle{}));

    Verify(Method(beeper_mock, beep).Using(Beeper::Signal::ERROR)).Once();

    // 4. Check recovery
    uint32_t recovery_time = 1000 + stove_config.data_timeout_ms + 2;
    set_time(recovery_time);
    When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(recovery_time);
    supervisor.update();
    Verify(Method(beeper_mock, beep).Using(Beeper::Signal::NONE)).Once();
  }

  SUBCASE("PID integration") {
    // 1. Exit bypass
    supervisor.takeSnapshot();

    // 2. Setup mocks for PID mode update
    When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(1000);
    set_time(1000);

    // 3. Dial at 50% with boost allowed
    StoveThrottle throttle_allow_boost = {0.5f, 2};
    When(Method(dial_mock, getThrottle)).AlwaysReturn(throttle_allow_boost);

    // 4. PID Output High (1.0) -> should translate to max boost
    When(Method(controller_mock, getLevel)).AlwaysReturn(1.0f);

    StoveThrottle captured_throttle = {};
    When(Method(actuator_mock, setThrottle)).Do([&](const StoveThrottle &t) {
      captured_throttle = t;
    });

    supervisor.update();

    // 5. Verify throttle passed to actuator
    // The boost calculation logic in supervisor should be respected.
    CHECK(captured_throttle.base == 0.5f);
    CHECK(captured_throttle.boost == 2);
  }
}

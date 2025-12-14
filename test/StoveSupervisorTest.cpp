#include <doctest.h>
#include <ArduinoFake.h>

#include "StoveSupervisor.h"
#include "StoveDial.h"
#include "StoveActuator.h"
#include "ThermalController.h"
#include "Beeper.h"
#include "TrendAnalyzer.h"
#include "DigiPot.h"

// Interfaces
#include "AnalogReadPin.h"
#include "DigitalWritePin.h"
#include "Buzzer.h"

using namespace fakeit;

TEST_CASE("StoveSupervisor Logic") {
  // Global Mocks
  Fake(Method(ArduinoFake(), millis));
  Fake(Method(ArduinoFake(), delayMicroseconds));

  ThrottleConfig throttle_config{.num_boosts = 2};
  StoveConfig stove_config;

  // --- Collaborator Mocks ---
  Mock<StoveDial> dial_mock;
  Mock<StoveActuator> actuator_mock;
  Mock<Beeper> beeper_mock;
  Mock<TrendAnalyzer> analyzer_mock;
  Mock<ThermalController> controller_mock;

  StoveSupervisor supervisor(dial_mock.get(), actuator_mock.get(),
                             controller_mock.get(), beeper_mock.get(),
                             analyzer_mock.get(), stove_config, throttle_config);

  auto set_time = [](uint32_t t) {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(t);
  };
  set_time(0);

  SUBCASE("Snapshot updates target temp") {
    // 1. Setup Dial to return 50%
    StoveThrottle throttle_50 = {0.5f, 0};
    When(Method(dial_mock, getThrottle)).AlwaysReturn(throttle_50);

    // 2. Expect Controller update
    Fake(Method(controller_mock, setTargetTemp));
    Fake(Method(beeper_mock, beep));

    supervisor.takeSnapshot();

    // Verify Target Temp: 20 + 0.5 * (120 - 20) = 70.0
    Verify(Method(controller_mock, setTargetTemp).Using(70.0f)).Exactly(1);
    Verify(Method(beeper_mock, beep).Using(Beeper::Signal::ACCEPT)).Exactly(1);
  }

  SUBCASE("Safety check stale data") {
    // 0. Establish healthy state to clear timeout flag
    When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(1000);
    set_time(1100);
    When(Method(dial_mock, getThrottle)).AlwaysReturn(StoveThrottle{});
    When(Method(controller_mock, getLevel)).AlwaysReturn(0.0f);
    Fake(Method(actuator_mock, setThrottle));
    supervisor.update();

    // 1. Setup Stale Data condition
    // Last update at 1000, current time 1000 + timeout + 100
    When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(1000);
    set_time(1000 + stove_config.data_timeout_ms + 100);

    // 2. Expect Safety Shutdown
    StoveThrottle captured_throttle = {1.0f, 1}; // Initialize with unsafe value
    When(Method(actuator_mock, setThrottle)).AlwaysDo([&](const StoveThrottle &throttle) {
      captured_throttle = throttle;
    });
    Fake(Method(beeper_mock, beep));
    Fake(Method(controller_mock, getLevel)); // Called but result ignored for safety
    When(Method(dial_mock, getThrottle)).AlwaysReturn(StoveThrottle{});

    supervisor.update();

    // Verify Actuator set to 0
    CHECK(captured_throttle.base == 0.0f);

    // Verify Alarm Beep (time aligned)
    set_time(70000);
    supervisor.update();
    Verify(Method(beeper_mock, beep).Using(Beeper::Signal::ERROR)).AtLeast(1);
  }

  SUBCASE("PID integration and clamping") {
    // 1. Dial at 50%
    StoveThrottle throttle_allow_boost = {0.5f, 2};
    When(Method(dial_mock, getThrottle)).AlwaysReturn(throttle_allow_boost);

    // 2. PID Output High (1.0)
    When(Method(controller_mock, getLevel)).AlwaysReturn(1.0f);
    When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(1000); // Fresh data
    set_time(1000);

    StoveThrottle captured_throttle = {};
    When(Method(actuator_mock, setThrottle)).AlwaysDo([&](const StoveThrottle &t) {
      captured_throttle = t;
    });

    supervisor.update();

    // 3. Verify Boost Logic
    // Base power ratio 0.8. PID 1.0.
    // Boost level = (1.0 - 0.8) / (1.0 - 0.8) = 1.0.
    // Num boosts = 2. Result = 2.
    CHECK(captured_throttle.boost == 2);
  }
}

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
#include "Thermometer.h"
#include "Logger.h"

// Interfaces
#include "AnalogReadPin.h"
#include "DigitalWritePin.h"
#include "Buzzer.h"
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

  // --- DUT ---
  StoveSupervisor supervisor(dial_mock.get(), actuator_mock.get(),
                             controller_mock.get(), beeper_mock.get(),
                             analyzer_mock.get(), thermometer_mock.get(),
                             stove_config, throttle_config);

  uint32_t current_time_ms = 0;
  When(Method(ArduinoFake(), millis)).AlwaysDo([&]() { return current_time_ms; });
  auto set_time = [&](uint32_t t) {
    current_time_ms = t;
  };
  set_time(0);

  // --- Common Stubs ---
  When(Method(dial_mock, getPosition)).AlwaysReturn(0.0f);
  When(Method(dial_mock, isOff)).AlwaysReturn(false);
  When(Method(dial_mock, isBoil)).AlwaysReturn(false);
  Fake(Method(dial_mock, update));
  Fake(Method(actuator_mock, setBypass));
  Fake(Method(actuator_mock, setThrottle));
  Fake(Method(actuator_mock, update));
  Fake(Method(beeper_mock, beep));
  Fake(Method(beeper_mock, update));
  When(Method(controller_mock, getPower)).AlwaysReturn(0.0f);
  Fake(Method(controller_mock, setTargetTemp));
  Fake(Method(controller_mock, update));
  Fake(Method(thermometer_mock, start));
  Fake(Method(thermometer_mock, stop));
  When(Method(thermometer_mock, connected)).AlwaysReturn(false);
  When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(0);

  auto reset_actuator = [&]() {
    actuator_mock.Reset();
    Fake(Method(actuator_mock, setBypass));
    Fake(Method(actuator_mock, setThrottle));
    Fake(Method(actuator_mock, update));
  };

  auto reset_thermometer = [&]() {
    thermometer_mock.Reset();
    Fake(Method(thermometer_mock, start));
    Fake(Method(thermometer_mock, stop));
    When(Method(thermometer_mock, connected)).AlwaysReturn(false);
  };

  auto reset_controller = [&]() {
    controller_mock.Reset();
    When(Method(controller_mock, getPower)).AlwaysReturn(0.0f);
    Fake(Method(controller_mock, setTargetTemp));
    Fake(Method(controller_mock, update));
  };

  SUBCASE("Initial state is SLEEP") {
    // In SLEEP, if dial is off, nothing happens.
    When(Method(dial_mock, isOff)).AlwaysReturn(true);
    supervisor.update();

    Verify(Method(thermometer_mock, start)).Never();
    Verify(Method(actuator_mock, setBypass)).Never();
  }

  SUBCASE("Transition SLEEP -> SCANNING") {
    When(Method(dial_mock, isOff)).AlwaysReturn(false); // Dial turned on
    When(Method(dial_mock, isBoil)).AlwaysReturn(false);

    supervisor.update();

    Verify(Method(thermometer_mock, start)).Once();
    Verify(Method(actuator_mock, setBypass)).Once();
  }

  SUBCASE("SCANNING behavior") {
    // Transition to SCANNING first
    When(Method(dial_mock, isOff)).AlwaysReturn(false);
    supervisor.update();
    Verify(Method(actuator_mock, setBypass)).Once(); // Entry

    // Clear history
    reset_actuator();
    reset_thermometer();

    SUBCASE("Transition SCANNING -> COOLDOWN on dial off") {
      When(Method(dial_mock, isOff)).AlwaysReturn(true);
      set_time(1001);
      supervisor.update();
      // COOLDOWN entry sets bypass
      Verify(Method(actuator_mock, setBypass)).Once();
    }

    SUBCASE("Transition SCANNING -> CONNECTED") {
      When(Method(thermometer_mock, connected)).AlwaysReturn(true);
      Fake(Method(beeper_mock, beep));
      supervisor.update();
      // CONNECTED entry sets bypass
      Verify(Method(actuator_mock, setBypass)).Once();
      Verify(Method(beeper_mock, beep).Using(Beeper::Signal::ACCEPT)).Once();
    }
  }

  SUBCASE("CONNECTED behavior") {
    // Get to CONNECTED
    When(Method(dial_mock, isOff)).AlwaysReturn(false);
    When(Method(thermometer_mock, connected)).AlwaysReturn(true);
    supervisor.update(); // SLEEP -> SCANNING
    supervisor.update(); // SCANNING -> CONNECTED
    reset_actuator();

    SUBCASE("Transition CONNECTED -> ACTIVATING") {
      When(Method(dial_mock, isBoil)).AlwaysReturn(true);
      beeper_mock.Reset();
      Fake(Method(beeper_mock, beep));
      Fake(Method(beeper_mock, update));

      supervisor.update();

      Verify(Method(beeper_mock, beep).Using(Beeper::Signal::ACCEPT)).Never();
      Verify(Method(actuator_mock, setBypass)).Once();
    }

    SUBCASE("Reconnection without sleep does not beep") {
      // Disconnect
      When(Method(thermometer_mock, connected)).AlwaysReturn(false);
      supervisor.update(); // SCANNING

      // Reconnect
      When(Method(thermometer_mock, connected)).AlwaysReturn(true);
      beeper_mock.Reset();
      Fake(Method(beeper_mock, beep));
      Fake(Method(beeper_mock, update));
      supervisor.update(); // CONNECTED

      Verify(Method(beeper_mock, beep)).Never();
    }

    SUBCASE("Transition CONNECTED -> SCANNING on disconnect") {
      thermometer_mock.Reset();
      Fake(Method(thermometer_mock, start));
      Fake(Method(thermometer_mock, stop));
      When(Method(thermometer_mock, connected)).AlwaysReturn(false);

      supervisor.update();
      Verify(Method(thermometer_mock, start)).Once();
    }
  }

  SUBCASE("ACTIVATING behavior") {
    // Get to ACTIVATING
    When(Method(dial_mock, isOff)).AlwaysReturn(false);
    When(Method(thermometer_mock, connected)).AlwaysReturn(true);
    When(Method(dial_mock, isBoil)).AlwaysReturn(true);
    Fake(Method(beeper_mock, beep));
    supervisor.update(); // SLEEP -> SCANNING
    supervisor.update(); // SCANNING -> CONNECTED
    supervisor.update(); // CONNECTED -> ACTIVATING
    reset_actuator();

    SUBCASE("Waits 3 seconds") {
      set_time(2999);
      supervisor.update();
      Verify(Method(actuator_mock, setThrottle)).Never();
    }

    SUBCASE("Transition ACTIVATING -> ACTIVE") {
      set_time(3001);
      supervisor.update();
      // ACTIVE entry sets throttle to 0
      Verify(Method(actuator_mock, setThrottle)).Once();
    }
  }

  SUBCASE("ACTIVE behavior") {
    // Fast forward to ACTIVE
    When(Method(dial_mock, isOff)).AlwaysReturn(false);
    When(Method(thermometer_mock, connected)).AlwaysReturn(true);
    When(Method(dial_mock, isBoil)).AlwaysReturn(true);
    Fake(Method(beeper_mock, beep));
    supervisor.update(); // Wake
    supervisor.update(); // Connected
    supervisor.update(); // Activating
    set_time(3001);
    supervisor.update(); // Active

    reset_actuator();
    reset_controller();

    SUBCASE("Holds 0 for 300ms") {
      set_time(3001 + 299);
      supervisor.update();
      Verify(Method(actuator_mock, setThrottle)).Never(); // {0,0} set on entry
    }

    SUBCASE("PID Control Loop after 300ms") {
      set_time(3001 + 301);
      When(Method(dial_mock, getPosition)).AlwaysReturn(0.5f);
      When(Method(controller_mock, getPower)).AlwaysReturn(0.4f);

      supervisor.update();

      Verify(Method(controller_mock, setTargetTemp)).Once();
      Verify(Method(controller_mock, update)).Once();
      Verify(Method(actuator_mock, setThrottle)).Once();
    }

    SUBCASE("Transition ACTIVE -> DISCONNECTED on signal loss") {
      set_time(3001 + 30001);
      beeper_mock.Reset();
      Fake(Method(beeper_mock, update));
      Fake(Method(beeper_mock, beep));

      When(Method(beeper_mock, beep)).AlwaysDo([&](Beeper::Signal s) {
        CHECK(s == Beeper::Signal::ERROR);
      });

      When(Method(actuator_mock, setThrottle)).AlwaysDo([&](const StoveThrottle &t) {
        CHECK(isNear(t, StoveThrottle{0.0f, 0}));
      });

      supervisor.update();

      Verify(Method(actuator_mock, setThrottle)).Once();
      Verify(Method(beeper_mock, beep)).Once();

      SUBCASE("Transition DISCONNECTED -> ACTIVE on signal recovery") {
        When(Method(analyzer_mock, getLastUpdateMs)).AlwaysReturn(3001 + 30001);
        When(Method(dial_mock, getPosition)).AlwaysReturn(0.5f);

        When(Method(actuator_mock, setThrottle)).AlwaysDo([&](const StoveThrottle &t) {
          CHECK(isNear(t, StoveThrottle{0.4f, 0}));
        });

        supervisor.update();
        Verify(Method(actuator_mock, setThrottle)).Exactly(2);
      }
    }

    SUBCASE("Transition ACTIVE -> COOLDOWN") {
      When(Method(dial_mock, isOff)).AlwaysReturn(true);
      set_time(3001 + 1001);
      supervisor.update();
      // COOLDOWN entry sets bypass
      Verify(Method(actuator_mock, setBypass)).Once();
    }
  }

  SUBCASE("COOLDOWN behavior") {
    // Get to COOLDOWN
    When(Method(dial_mock, isOff)).AlwaysReturn(false);
    supervisor.update(); // SCANNING
    When(Method(dial_mock, isOff)).AlwaysReturn(true);
    set_time(1001);
    supervisor.update(); // COOLDOWN
    reset_actuator();
    reset_thermometer();

    SUBCASE("Transition COOLDOWN -> SLEEP on timeout") {
      set_time(30001);
      supervisor.update();
      Verify(Method(thermometer_mock, stop)).Once();
    }

    SUBCASE("Transition COOLDOWN -> SCANNING on dial on") {
      When(Method(dial_mock, isOff)).AlwaysReturn(false);
      supervisor.update();
      Verify(Method(thermometer_mock, start)).Once();
    }
  }
}

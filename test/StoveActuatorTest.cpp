#include "StoveActuator.h"
#include "DigiPot.h"
#include "DigitalWritePin.h"
#include <ArduinoFake.h>
#include <doctest.h>

using namespace fakeit;

TEST_CASE("StoveActuator Logic") {

  Fake(Method(ArduinoFake(), delayMicroseconds));
  Mock<DigiPot> pot_mock;

  StoveLevelConfig config{.min = 0.1f, .max = 0.8f, .boost = 0.9f, .num_boosts = 2};

  StoveActuator actuator(pot_mock.get(), config);

  SUBCASE("Normal operation (no boost)") {
    StoveLevel level{.base = 0.5f, .boost = 0};
    actuator.setLevel(level);

    // Expect setLevel to be called with base level
    Fake(Method(pot_mock, setLevel));

    actuator.update();

    Verify(Method(pot_mock, setLevel).Using(0.5f)).Exactly(1);
  }

  SUBCASE("Boost activation") {
    StoveLevel level{.base = 0.5f, .boost = 2};
    actuator.setLevel(level);

    // Mock getLevel to return a normal value (<= config.boost)
    When(Method(pot_mock, getLevel)).AlwaysReturn(0.5f);
    Fake(Method(pot_mock, setLevel));

    // First update: current_boost (0) < target (2).
    // pot level (0.5) <= config.boost (0.9).
    // Should set pot to 1.0 and increment current_boost to 1.
    actuator.update();
    Verify(Method(pot_mock, setLevel).Using(1.0f)).Exactly(1);

    // Second update: current_boost (1) < target (2).
    // Should set pot to 1.0 and increment current_boost to 2.
    actuator.update();
    Verify(Method(pot_mock, setLevel).Using(1.0f)).Exactly(2);

    // Third update: current_boost (2) == target (2).
    // Should set pot to target base (0.5).
    actuator.update();
    Verify(Method(pot_mock, setLevel).Using(0.5f)).Exactly(1);
  }

  SUBCASE("Boost cancellation") {
    // 1. Get to boost state first
    StoveLevel level{.base = 0.5f, .boost = 1};
    actuator.setLevel(level);
    When(Method(pot_mock, getLevel)).AlwaysReturn(0.5f);
    Fake(Method(pot_mock, setLevel));

    actuator.update(); // current_boost becomes 1
    Verify(Method(pot_mock, setLevel).Using(1.0f)).Exactly(1);

    // 2. Cancel boost
    level.boost = 0;
    actuator.setLevel(level);

    actuator.update();

    // Logic: below_max_level = config.max - (config.boost - config.max) / 2
    // below_max_level = 0.8 - (0.9 - 0.8)/2 = 0.75.
    // pot.setLevel(min(0.75, 0.5)) -> 0.5.
    Verify(Method(pot_mock, setLevel).Using(0.5f)).Exactly(1);
  }

  SUBCASE("Already high level (unreachable with standard config)") {
    StoveLevel level{.base = 0.5f, .boost = 1};
    actuator.setLevel(level);

    // Mock getLevel to be > config.boost (0.9).
    // This forces the 'else' branch in update().
    When(Method(pot_mock, getLevel)).AlwaysReturn(1.0f);
    Fake(Method(pot_mock, setLevel));

    actuator.update();

    // Logic: above_max_level = config.max + (config.boost - config.max) / 2
    // above_max_level = 0.8 + (0.9 - 0.8)/2 = 0.85.
    Verify(Method(pot_mock, setLevel).Using(0.85f)).Exactly(1);
  }
}

#include "StoveActuator.h"
#include "DigiPot.h"
#include "DigitalWritePin.h"
#include <ArduinoFake.h>
#include <doctest.h>

using namespace fakeit;

TEST_CASE("StoveActuator Logic") {

  Fake(Method(ArduinoFake(), delayMicroseconds));
  Fake(Method(ArduinoFake(), millis));
  Mock<DigiPot> pot_mock;

  ThrottleConfig config{.min = 0.1f, .max = 0.8f, .boost = 0.9f, .num_boosts = 2};

  StoveActuator actuator(pot_mock.get(), config);
  // NOTE: After construction, actuator is in "direct mode"

  SUBCASE("setPosition sets direct mode and position") {
    When(Method(pot_mock, setPosition)).AlwaysReturn();
    actuator.setPosition(0.5f);
    Verify(Method(pot_mock, setPosition).Using(0.5f)).Once();

    // Calling setThrottle should now start in direct mode
    StoveThrottle throttle{.base = 0.5f, .boost = 0};
    actuator.setThrottle(throttle);

    // Enters direct mode, so it should set position based on throttle.
    // position = 0.5 * 0.8 = 0.4
    // below_max_level = 0.8 - (0.9-0.8)/2 = 0.75
    // setPosition(min(0.75, 0.4))
    Verify(Method(pot_mock, setPosition).Using(0.4f)).Once();
  }

  SUBCASE("Normal operation (no boost)") {
    StoveThrottle throttle{.base = 0.5f, .boost = 0};

    Fake(Method(pot_mock, setPosition));
    When(Method(ArduinoFake(), millis)).AlwaysReturn(2000);

    // First call is in direct mode.
    actuator.setThrottle(throttle);

    // position = 0.5 * 0.8 = 0.4
    // below_max_level = 0.75
    // setPosition(min(0.75, 0.4)) -> 0.4
    Verify(Method(pot_mock, setPosition).Using(0.4f)).Once();

    // Call again, no longer in direct mode.
    // throttle.boost (0) == current_boost_ (0)
    When(Method(ArduinoFake(), millis)).AlwaysReturn(3001); // > 1000ms later
    actuator.setThrottle(throttle);
    Verify(Method(pot_mock, setPosition).Using(0.4f)).Twice();
  }

  SUBCASE("Boost activation") {
    StoveThrottle throttle{.base = 1.0f, .boost = 2};
    const float below_max_level = config.max - (config.boost - config.max) / 2; // 0.75f
    const float above_max_level = config.max + (config.boost - config.max) / 2; // 0.85f

    Fake(Method(pot_mock, setPosition));
    When(Method(pot_mock, getPosition)).AlwaysReturn(below_max_level);

    // 1. First call. is_direct_mode_ = true.
    When(Method(ArduinoFake(), millis)).Return(10000);
    actuator.setThrottle(throttle);
    // Enters direct mode, setting position and disabling direct mode
    Verify(Method(pot_mock, setPosition).Using(below_max_level)).Once();

    // 2. Second call, start boosting.
    When(Method(ArduinoFake(), millis)).Return(11001);
    actuator.setThrottle(throttle);
    Verify(Method(pot_mock, setPosition).Using(1.0f)).Once();

    // 3. Third call, continue boosting.
    When(Method(ArduinoFake(), millis)).Return(12002);
    actuator.setThrottle(throttle);
    Verify(Method(pot_mock, setPosition).Using(1.0f)).Twice();

    // 4. Fourth call, boost is done, set to base position
    When(Method(ArduinoFake(), millis)).Return(13003);
    actuator.setThrottle(throttle);
    Verify(Method(pot_mock, setPosition).Using(above_max_level)).Once();
  }

  SUBCASE("Boost cancellation") {
    Fake(Method(pot_mock, setPosition));
    const float position = 0.5f * config.max; // 0.4f
    const float below_max_level = config.max - (config.boost - config.max) / 2; // 0.75

    When(Method(pot_mock, getPosition)).AlwaysReturn(below_max_level);

    // 1. Get to boost state first
    StoveThrottle boost_throttle{.base = 1.0f, .boost = 1};
    // First call, direct mode.
    When(Method(ArduinoFake(), millis)).AlwaysReturn(10000);
    actuator.setThrottle(boost_throttle);
    Verify(Method(pot_mock, setPosition).Using(below_max_level)).Once();

    // Second call, boosting
    When(Method(ArduinoFake(), millis)).AlwaysReturn(11001);
    actuator.setThrottle(boost_throttle);
    Verify(Method(pot_mock, setPosition).Using(1.0f)).Once(); // current_boost becomes 1

    // 2. Cancel boost
    StoveThrottle zero_throttle{.base = 0.5f, .boost = 0};
    When(Method(ArduinoFake(), millis)).AlwaysReturn(11002);
    actuator.setThrottle(zero_throttle);

    // Logic: throttle.boost (0) < current_boost_ (1).
    // below_max_level = 0.75.
    // pot.setPosition(min(0.75, 0.4)) -> 0.4.
    Verify(Method(pot_mock, setPosition).Using(position)).Once();
  }
}

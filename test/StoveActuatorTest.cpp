#include "StoveActuator.h"
#include "Potentiometer.h"
#include "DigitalWritePin.h"
#include <ArduinoFake.h>
#include <doctest.h>

using namespace fakeit;

TEST_CASE("StoveActuator Logic") {

  Fake(Method(ArduinoFake(), delayMicroseconds));
  Fake(Method(ArduinoFake(), millis));
  Mock<Potentiometer> potentiometer_mock;
  Mock<DigitalWritePin> bypass_mock;

  Fake(Method(bypass_mock, set));
  Fake(Method(potentiometer_mock, setValue));

  ThrottleConfig config;

  StoveActuator actuator(potentiometer_mock.get(), bypass_mock.get(), config);
  // NOTE: After construction, actuator is in bypass mode.

  SUBCASE("setBypass sets bypass mode") {
    actuator.setBypass();
    Verify(Method(bypass_mock, set).Using(PinState::Low));

    // Calling setThrottle should now start in bypass
    StoveThrottle throttle{.position = 0.5f, .boost = 0};
    actuator.setThrottle(throttle);

    // Enters bypass, so it should set value based on throttle.
    // value = 0.5 * config.max
    Verify(Method(potentiometer_mock, setValue).Using(0.5f * config.max)).Once();
  }

  SUBCASE("Normal operation (no boost)") {
    StoveThrottle throttle{.position = 0.5f, .boost = 0};

    When(Method(ArduinoFake(), millis)).AlwaysReturn(2000);

    // First call is in bypass.
    actuator.setThrottle(throttle);

    // value = 0.5 * config.max
    Verify(Method(potentiometer_mock, setValue).Using(0.5f * config.max)).Once();

    // Call again, no longer in bypass.
    // throttle.boost (0) == current_boost_ (0)
    When(Method(ArduinoFake(), millis)).AlwaysReturn(3001); // > 1000ms later
    actuator.setThrottle(throttle);
    Verify(Method(potentiometer_mock, setValue).Using(0.5f * config.max)).Twice();
  }

  SUBCASE("Boost activation") {
    StoveThrottle throttle{.position = 1.0f, .boost = 2};
    StoveThrottle throttle_reset{.position = 1.0f, .boost = 0};
    const float arm_value = config.max + (config.arm - config.max) / 2; // 0.84f

    // 1. First call. is_bypass_ = true.
    When(Method(ArduinoFake(), millis)).Return(10000);
    actuator.setThrottle(throttle_reset);
    // Enters bypass, setting value and disabling bypass
    Verify(Method(potentiometer_mock, setValue).Using(config.max)).Once();

    // 2. Second call, start boosting.
    When(Method(ArduinoFake(), millis)).Return(11001);
    actuator.setThrottle(throttle);
    Verify(Method(potentiometer_mock, setValue).Using(1.0f)).Once();

    // 3. Third call, continue boosting.
    // Logic toggles: High -> Low (arm_value)
    When(Method(ArduinoFake(), millis)).Return(12002);
    actuator.setThrottle(throttle);
    Verify(Method(potentiometer_mock, setValue).Using(arm_value)).Once();

    // 4. Fourth call, pulse high again to reach boost 2
    When(Method(ArduinoFake(), millis)).Return(13003);
    actuator.setThrottle(throttle);
    Verify(Method(potentiometer_mock, setValue).Using(1.0f)).Twice();

    // 5. Fifth call, finish pulse, increment boost to 2.
    // Logic toggles: High -> Low (arm_value)
    When(Method(ArduinoFake(), millis)).Return(14004);
    actuator.setThrottle(throttle);
    Verify(Method(potentiometer_mock, setValue).Using(arm_value)).Twice();

    // 6. Sixth call, steady state (boost 2 == boost 2).
    // Should maintain value (arm_value) and NOT reset.
    When(Method(ArduinoFake(), millis)).Return(15005);
    actuator.setThrottle(throttle);
    Verify(Method(potentiometer_mock, setValue).Using(arm_value)).Twice();
  }

  SUBCASE("Boost cancellation") {
    const float value = 0.5f * config.max;
    const float deboost_value = config.max - (config.arm - config.max) / 2; // 0.76

    // 1. Get to boost state first
    StoveThrottle boost_throttle{.position = 1.0f, .boost = 1};
    // First call, bypass.
    When(Method(ArduinoFake(), millis)).AlwaysReturn(10000);
    actuator.setThrottle(boost_throttle);
    Verify(Method(potentiometer_mock, setValue).Using(deboost_value)).Once();

    // Second call, boosting
    When(Method(ArduinoFake(), millis)).AlwaysReturn(11001);
    actuator.setThrottle(boost_throttle);
    Verify(Method(potentiometer_mock, setValue).Using(1.0f)).Once(); // current_boost becomes 1

    // 2. Cancel boost
    StoveThrottle zero_throttle{.position = 0.5f, .boost = 0};
    When(Method(ArduinoFake(), millis)).AlwaysReturn(11002);
    actuator.setThrottle(zero_throttle);

    // Logic: throttle.boost (0) < current_boost_ (1).
    // deboost_value = 0.75.
    // pot.setPosition(min(0.75, 0.4)) -> 0.4.
    Verify(Method(potentiometer_mock, setValue).Using(value)).Once();
  }
}

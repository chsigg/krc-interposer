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

    // 1. First call. is_bypass_ = true.
    When(Method(ArduinoFake(), millis)).Return(10000);
    actuator.setThrottle(throttle_reset);
    // Enters bypass, setting value and disabling bypass
    Verify(Method(potentiometer_mock, setValue).Using(config.max)).Once();

    // 2. Second call, start boosting.
    // Logic: setValue(config.boost)
    When(Method(ArduinoFake(), millis)).Return(11001);
    actuator.setThrottle(throttle);
    Verify(Method(potentiometer_mock, setValue).Using(config.boost)).Once();

    // 3. Third call, continue boosting.
    // Logic: setValue(config.max), ++current_boost_
    When(Method(ArduinoFake(), millis)).Return(12002);
    actuator.setThrottle(throttle);
    Verify(Method(potentiometer_mock, setValue).Using(config.max)).Twice();

    // 4. Fourth call, pulse high again to reach boost 2
    // Logic: setValue(config.boost)
    When(Method(ArduinoFake(), millis)).Return(13003);
    actuator.setThrottle(throttle);
    Verify(Method(potentiometer_mock, setValue).Using(config.boost)).Twice();

    // 5. Fifth call, finish pulse, increment boost to 2.
    // Logic: setValue(config.max), ++current_boost_
    When(Method(ArduinoFake(), millis)).Return(14004);
    actuator.setThrottle(throttle);
    Verify(Method(potentiometer_mock, setValue).Using(config.max)).Exactly(3);

    // 6. Sixth call, steady state (boost 2 == boost 2).
    // Should maintain value (config.max) and NOT reset.
    When(Method(ArduinoFake(), millis)).Return(15005);
    actuator.setThrottle(throttle);
    Verify(Method(potentiometer_mock, setValue).Using(config.max)).Exactly(4);
  }

  SUBCASE("Boost cancellation") {
    const float value = 0.5f * config.max;

    // 1. Get to boost state first
    StoveThrottle boost_throttle{.position = 1.0f, .boost = 1};
    // First call, start boosting
    When(Method(ArduinoFake(), millis)).AlwaysReturn(10000);
    actuator.setThrottle(boost_throttle);
    Verify(Method(potentiometer_mock, setValue).Using(config.boost)).Once();

    // Second call, finish pulse, current_boost becomes 1
    When(Method(ArduinoFake(), millis)).AlwaysReturn(11001);
    actuator.setThrottle(boost_throttle);
    Verify(Method(potentiometer_mock, setValue).Using(config.max)).Once();

    // 2. Cancel boost
    StoveThrottle zero_throttle{.position = 0.5f, .boost = 0};
    When(Method(ArduinoFake(), millis)).AlwaysReturn(12002);
    actuator.setThrottle(zero_throttle);

    // Logic: throttle.boost (0) < current_boost_ (1).
    // pot.setPosition(min(0.4, 0.7)) -> 0.4.
    Verify(Method(potentiometer_mock, setValue).Using(value)).Once();
  }
}

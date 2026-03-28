#include "StoveActuator.h"
#include "AnalogWritePin.h"
#include "DigitalWritePin.h"
#include <ArduinoFake.h>
#include <doctest.h>

using namespace fakeit;

TEST_CASE("StoveActuator Logic") {

  Fake(Method(ArduinoFake(), delayMicroseconds));
  Fake(Method(ArduinoFake(), millis));
  Mock<AnalogWritePin> pwm_mock;

  Fake(Method(pwm_mock, write));

  ThrottleConfig config;

  StoveActuator actuator(pwm_mock.get(), config);

  SUBCASE("Minimum output clamping") {
    // Request a value below the threshold (or even 0)
    actuator.setThrottle({.position = 0.0f, .boost = 0});
    Verify(Method(pwm_mock, write).Using(config.min)).Once();

    // Request a value slightly above the threshold
    float above_pos = config.min * 1.1f;
    actuator.setThrottle({.position = above_pos / config.max, .boost = 0});
    Verify(Method(pwm_mock, write).Using(above_pos)).Once();
  }

  SUBCASE("Normal operation (no boost)") {
    StoveThrottle throttle{.position = 0.5f, .boost = 0};

    When(Method(ArduinoFake(), millis)).AlwaysReturn(2000);

    actuator.setThrottle(throttle);

    // value = 0.5 * config.max
    Verify(Method(pwm_mock, write).Using(0.5f * config.max)).Once();

    When(Method(ArduinoFake(), millis)).AlwaysReturn(3001);
    actuator.setThrottle(throttle);
    Verify(Method(pwm_mock, write).Using(0.5f * config.max)).Twice();
  }

  SUBCASE("Boost activation") {
    StoveThrottle throttle{.position = 1.0f, .boost = 2};
    StoveThrottle throttle_reset{.position = 1.0f, .boost = 0};

    // 1. First call. current_boost_ = 0.
    When(Method(ArduinoFake(), millis)).Return(10000);
    actuator.setThrottle(throttle_reset);
    Verify(Method(pwm_mock, write).Using(config.max)).Once();

    // 2. Second call, start boosting.
    // Logic: write(config.boost)
    When(Method(ArduinoFake(), millis)).Return(11001);
    actuator.setThrottle(throttle);
    Verify(Method(pwm_mock, write).Using(config.boost)).Once();

    // 3. Third call, continue boosting.
    // Logic: write(config.max), ++current_boost_
    When(Method(ArduinoFake(), millis)).Return(12002);
    actuator.setThrottle(throttle);
    Verify(Method(pwm_mock, write).Using(config.max)).Twice();

    // 4. Fourth call, pulse high again to reach boost 2
    // Logic: write(config.boost)
    When(Method(ArduinoFake(), millis)).Return(13003);
    actuator.setThrottle(throttle);
    Verify(Method(pwm_mock, write).Using(config.boost)).Twice();

    // 5. Fifth call, finish pulse, increment boost to 2.
    // Logic: write(config.max), ++current_boost_
    When(Method(ArduinoFake(), millis)).Return(14004);
    actuator.setThrottle(throttle);
    Verify(Method(pwm_mock, write).Using(config.max)).Exactly(3);

    // 6. Sixth call, steady state (boost 2 == boost 2).
    // Should maintain value (config.max) and NOT reset.
    When(Method(ArduinoFake(), millis)).Return(15005);
    actuator.setThrottle(throttle);
    Verify(Method(pwm_mock, write).Using(config.max)).Exactly(4);
  }

  SUBCASE("Boost cancellation") {
    const float value = 0.5f * config.max;

    // 1. Get to boost state first
    StoveThrottle boost_throttle{.position = 1.0f, .boost = 1};
    // First call, start boosting
    When(Method(ArduinoFake(), millis)).AlwaysReturn(10000);
    actuator.setThrottle(boost_throttle);
    Verify(Method(pwm_mock, write).Using(config.boost)).Once();

    // Second call, finish pulse, current_boost becomes 1
    When(Method(ArduinoFake(), millis)).AlwaysReturn(11001);
    actuator.setThrottle(boost_throttle);
    Verify(Method(pwm_mock, write).Using(config.max)).Once();

    // 2. Cancel boost
    StoveThrottle zero_throttle{.position = 0.5f, .boost = 0};
    When(Method(ArduinoFake(), millis)).AlwaysReturn(12002);
    actuator.setThrottle(zero_throttle);

    // Logic: throttle.boost (0) < current_boost_ (1).
    // writes deboost_value = min(value, config.max - 0.1)
    Verify(Method(pwm_mock, write).Using(std::min(value, config.max - 0.1f))).Once();
  }
}

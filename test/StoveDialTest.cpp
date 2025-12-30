#include "StoveDial.h"
#include "AnalogReadPin.h"
#include <ArduinoFake.h>
#include <doctest.h>

using namespace fakeit;

TEST_CASE("StoveDial Logic") {
  Mock<AnalogReadPin> pin_mock;
  ThrottleConfig config;
  StoveDial dial(pin_mock.get(), config);

  // Helper to stabilize the moving average (size 4)
  auto set_reading = [&](float val) {
    When(Method(pin_mock, read)).AlwaysDo([&] { return val; });
    for (int i = 0; i < 4; ++i) {
      dial.update();
    }
  };

  SUBCASE("ThrottleConfig") {
    auto values = {config.min, config.max, config.arm, config.boost, config.boil};
    CHECK(std::is_sorted(values.begin(), values.end()));
  }

  SUBCASE("Initialization") {
    CHECK(dial.getPosition() == 0.0f);
  }

  SUBCASE("isOff Logic") {
    SUBCASE("Is off") {
      set_reading(config.min - 0.05f);
      CHECK(dial.isOff());
    }

    SUBCASE("Is on") {
      set_reading(config.min);
      CHECK_FALSE(dial.isOff());

      set_reading(config.max);
      CHECK_FALSE(dial.isOff());
    }
  }

  SUBCASE("Throttle Mapping") {
    SUBCASE("Below min") {
      set_reading(config.min / 2);
      CHECK(dial.getPosition() == 0.0f);
    }

    SUBCASE("Linear range") {
      set_reading(config.min); // At min
      CHECK(dial.getPosition() == doctest::Approx(config.min / config.max));

      float mid = (config.min + config.max) / 2;
      set_reading(mid);
      CHECK(dial.getPosition() == doctest::Approx(mid / config.max));

      set_reading(config.max);
      CHECK(dial.getPosition() == doctest::Approx(1.0));
    }

    SUBCASE("Above max") {
      set_reading(config.max + 0.05f);
      CHECK(dial.getPosition() == doctest::Approx(1.0f));
    }
  }
}

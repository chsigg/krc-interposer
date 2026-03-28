#include "StoveDial.h"
#include "AnalogReadPin.h"
#include <ArduinoFake.h>
#include <doctest.h>

using namespace fakeit;

TEST_CASE("StoveDial Logic") {
  Mock<AnalogReadPin> pin_mock;
  ThrottleConfig config;
  StoveDial dial(pin_mock.get(), config);

  auto set_reading = [&](float val) {
    When(Method(pin_mock, read)).AlwaysReturn(val);
    dial.update();
  };

  SUBCASE("ThrottleConfig") {
    auto values = {config.min, config.max, config.boost, config.boil};
    CHECK(std::is_sorted(values.begin(), values.end()));
  }

  SUBCASE("Initialization") { CHECK(dial.getPosition() == 0.0f); }

  SUBCASE("isOff Logic") {
    SUBCASE("Is off") {
      set_reading(config.off - 0.01f);
      CHECK(dial.isOff());
    }

    SUBCASE("Is on") {
      set_reading(config.off + 0.01f);
      CHECK_FALSE(dial.isOff());
    }
  }

  SUBCASE("Throttle Mapping") {
    SUBCASE("Linear range") {
      set_reading(config.min);
      CHECK(dial.getPosition() == doctest::Approx(config.min / config.max));

      set_reading(config.max);
      CHECK(dial.getPosition() == doctest::Approx(1.0));
    }

    SUBCASE("Above max is ignored") {
      set_reading(config.min);
      float pos_at_min = dial.getPosition();

      set_reading(config.boil);
      CHECK(dial.getPosition() == pos_at_min);
    }
  }
}

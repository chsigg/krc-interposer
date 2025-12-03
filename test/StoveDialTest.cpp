#include "StoveDial.h"
#include "AnalogReadPin.h"
#include <ArduinoFake.h>
#include <doctest.h>

using namespace fakeit;

TEST_CASE("StoveDial Logic") {
  Mock<AnalogReadPin> pin_mock;
  LevelConfig config{.min = 0.1f, .max = 0.8f, .boost = 0.9f, .num_boosts = 2};
  StoveDial dial(pin_mock.get(), config);

  // Helper to stabilize the moving average (size 4)
  auto set_reading = [&](float val) {
    When(Method(pin_mock, read)).AlwaysDo([&] { return val; });
    for (int i = 0; i < 4; ++i) {
      dial.update();
    }
  };

  SUBCASE("Initialization") {
    CHECK(dial.getLevel().base == 0.0f);
    CHECK(dial.getLevel().boost == 0);
  }

  SUBCASE("Level Mapping") {
    SUBCASE("Below min") {
      set_reading(0.05f);
      CHECK(dial.getLevel().base == 0.0f);
    }

    SUBCASE("Linear range") {
      set_reading(0.1f); // At min
      // 0.1 / 0.8 = 0.125
      CHECK(dial.getLevel().base == doctest::Approx(0.125f));

      set_reading(0.5f);
      // 0.5 / 0.8 = 0.625
      CHECK(dial.getLevel().base == doctest::Approx(0.625f));

      set_reading(0.8f);
      // 0.8 / 0.8 = 1.0
      CHECK(dial.getLevel().base == doctest::Approx(1.0));
    }

    SUBCASE("Above max") {
      set_reading(0.85f);
      CHECK(dial.getLevel().base == doctest::Approx(1.0f));
    }
  }

  SUBCASE("Boost Logic") {
    // Start in normal range
    set_reading(0.85f);
    CHECK(dial.getLevel().boost == 0);

    // Enter boost zone
    set_reading(0.95f);
    CHECK(dial.getLevel().boost == 1);

    // Stay in boost zone (should not increment)
    set_reading(9.1f);
    CHECK(dial.getLevel().boost == 1);

    // Drop to re-arm zone (between max and boost)
    set_reading(0.85f);
    CHECK(dial.getLevel().boost == 1);

    // Enter boost zone again
    set_reading(0.95f);
    CHECK(dial.getLevel().boost == 2);

    // Reset by dropping below max
    set_reading(0.75f);
    CHECK(dial.getLevel().boost == 0);
  }
}

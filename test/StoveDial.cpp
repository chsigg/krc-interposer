#include "StoveDial.h"
#include "AnalogReadPin.h"
#include <doctest.h>
#include <fakeit.hpp>

using namespace fakeit;

TEST_CASE("StoveDial Logic") {
  Mock<AnalogReadPin> pin_mock;
  StoveLevelConfig config{.min = 10, .max = 100, .boost = 150, .num_boosts = 2};
  StoveDial dial(pin_mock.get(), config);

  // Helper to stabilize the moving average (size 4)
  auto set_reading = [&](int val) {
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
      set_reading(5);
      CHECK(dial.getLevel().base == 0.0f);
    }

    SUBCASE("Linear range") {
      set_reading(10); // At min
      // 10 / 100 = 0.1
      CHECK(dial.getLevel().base == doctest::Approx(0.1f));

      set_reading(50);
      // Implementation uses reading / max => 50 / 100 = 0.5
      CHECK(dial.getLevel().base == doctest::Approx(0.5f));
    }

    SUBCASE("Above max") {
      set_reading(100);
      CHECK(dial.getLevel().base == doctest::Approx(1.0f));

      set_reading(120);
      CHECK(dial.getLevel().base == doctest::Approx(1.0f));
    }
  }

  SUBCASE("Boost Logic") {
    // Start in normal range
    set_reading(50);
    CHECK(dial.getLevel().boost == 0);

    // Enter boost zone
    set_reading(150);
    CHECK(dial.getLevel().boost == 1);

    // Stay in boost zone (should not increment)
    set_reading(155);
    CHECK(dial.getLevel().boost == 1);

    // Drop to re-arm zone (between max and boost)
    set_reading(120);
    CHECK(dial.getLevel().boost == 1);

    // Enter boost zone again
    set_reading(150);
    CHECK(dial.getLevel().boost == 2);

    // Reset by dropping below max
    set_reading(99);
    CHECK(dial.getLevel().boost == 0);
  }
}

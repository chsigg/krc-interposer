#include <doctest.h>
#include "TrendAnalyzer.h"

TEST_CASE("TrendAnalyzer Logic") {
  TrendAnalyzer ta;

  SUBCASE("Initial state") {
    CHECK(ta.getValue(0) == 0.0f);
    CHECK(ta.getSlope() == 0.0f);
  }

  SUBCASE("Single reading") {
    ta.addReading(10.0f, 1000);
    CHECK(ta.getValue(1000) == 10.0f);
    CHECK(ta.getSlope() == 0.0f);
  }

  SUBCASE("Two readings - constant") {
    ta.addReading(10.0f, 1000);
    ta.addReading(10.0f, 2000);
    CHECK(ta.getValue(2000) == 10.0f);
    CHECK(ta.getSlope() == 0.0f);
  }

  SUBCASE("Two readings - increasing") {
    ta.addReading(10.0f, 1000);
    ta.addReading(20.0f, 2000);
    // Slope = (20-10)/(2000-1000) = 0.01
    CHECK(ta.getValue(2000) == doctest::Approx(20.0f));
    CHECK(ta.getSlope() == doctest::Approx(0.01f));
  }

  SUBCASE("Two readings - decreasing") {
    ta.addReading(20.0f, 1000);
    ta.addReading(10.0f, 2000);
    // Slope = -0.01
    CHECK(ta.getValue(2000) == doctest::Approx(10.0f));
    CHECK(ta.getSlope() == doctest::Approx(-0.01f));
  }

  SUBCASE("Three readings - perfect line") {
    ta.addReading(10.0f, 1000);
    ta.addReading(20.0f, 2000);
    ta.addReading(30.0f, 3000);

    CHECK(ta.getValue(3000) == doctest::Approx(30.0f));
    CHECK(ta.getSlope() == doctest::Approx(0.01f));
  }

  SUBCASE("Buffer overflow") {
    // Capacity is 15. Add 20 readings.
    // y = x.
    for (int i = 0; i < 20; ++i) {
      ta.addReading(static_cast<float>(i), i * 1000);
    }

    // Should have last 15 readings: 5 to 19.
    // Slope should still be 0.001.
    // Value should be 19.
    CHECK(ta.getValue(19000) == doctest::Approx(19.0f));
    CHECK(ta.getSlope() == doctest::Approx(0.001f));
  }

  SUBCASE("Out of order insertion") {
    // Insert 10, 30. Then insert 20.
    ta.addReading(10.0f, 1000);
    ta.addReading(30.0f, 3000);
    CHECK(ta.getValue(3000) == doctest::Approx(30.0f));

    ta.addReading(20.0f, 2000);
    // Should be perfect line now
    CHECK(ta.getValue(3000) == doctest::Approx(30.0f)); // Value at latest time (3000)
    CHECK(ta.getSlope() == doctest::Approx(0.01f));
  }

  SUBCASE("Timer wrap-around") {
    // t1 is just before wrap (MAX - 1000)
    // t2 is just after wrap (1000)
    // Delta time is 2001 ms (1000 - (-1001))
    uint32_t t1 = UINT32_MAX - 1000;
    uint32_t t2 = 1000;

    ta.addReading(10.0f, t1);
    ta.addReading(20.0f, t2);

    CHECK(ta.getValue(t2) == doctest::Approx(20.0f));
    // Slope = (20 - 10) / 2001 ~= 0.0049975
    CHECK(ta.getSlope() == doctest::Approx(0.0049975f).epsilon(0.001));
  }
  
  SUBCASE("Clear") {
    ta.addReading(10.0f, 1000);
    ta.addReading(20.0f, 2000);

    ta.clear();

    CHECK(ta.getLastUpdateMs() == 0);
    CHECK(ta.getValue(2000) == 0.0f);
    CHECK(ta.getSlope() == 0.0f);
  }
}
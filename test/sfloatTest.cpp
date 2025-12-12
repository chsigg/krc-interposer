#include "sfloat.h"
#include <doctest.h>

TEST_CASE("sfloat") {
  SUBCASE("testEncodeDecode") {
    float temp = 23.45f;
    auto encoded = encodeIEEE11073(temp);
    float decoded = decodeIEEE11073(encoded.data(), encoded.size());
    CHECK(decoded == doctest::Approx(temp));
  }
}

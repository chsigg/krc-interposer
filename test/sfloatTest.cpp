#include "sfloat.h"
#include <doctest.h>
#include <cmath>

TEST_CASE("sfloat") {
  SUBCASE("testEncodeDecode") {
    float temp = 23.45f;
    auto encoded = encodeIEEE11073(temp);
    float decoded = decodeIEEE11073(encoded.data(), encoded.size());
    CHECK(decoded == doctest::Approx(temp));
  }

  SUBCASE("testNaN") {
    float temp = std::numeric_limits<float>::quiet_NaN();
    auto encoded = encodeIEEE11073(temp);
    // IEEE 11073-20601 NaN: Mantissa 0x007FFFFF, Exponent 0
    // Byte order: [flags, m_low, m_mid, m_high, exponent]
    // {0x00, 0xFF, 0xFF, 0x7F, 0x00}
    CHECK(encoded[0] == 0x00);
    CHECK(encoded[1] == 0xFF);
    CHECK(encoded[2] == 0xFF);
    CHECK(encoded[3] == 0x7F);
    CHECK(encoded[4] == 0x00);

    float decoded = decodeIEEE11073(encoded.data(), encoded.size());
    CHECK(std::isnan(decoded));
  }
}

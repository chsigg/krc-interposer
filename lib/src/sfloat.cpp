#include "sfloat.h"
#include <cmath>
#include <cstring>
#include <limits>

static bool isNan(float temp) {
  uint32_t bits;
  std::memcpy(&bits, &temp, sizeof(bits));
  // IEEE 754 NaN: exponent (bits 23-30) is all 1s, mantissa (bits 0-22) is non-zero.
  return ((bits & 0x7F800000) == 0x7F800000) && ((bits & 0x007FFFFF) != 0);
}

std::array<uint8_t, 5> encodeIEEE11073(float temp) {
  if (isNan(temp)) {
    // NaN is represented as 0x007FFFFF in IEEE 11073-20601 FLOAT.
    // The exponent must be 0 for special values.
    return {0x00, 0xFF, 0xFF, 0x7F, 0x00};
  }

  int8_t exponent = -2;
  int32_t mantissa = (int32_t)round(temp * 100.0f);

  std::array<uint8_t, 5> result;
  result[0] = 0x0; // Flag byte: Celsius
  result[1] = (uint8_t)(mantissa & 0xFF);
  result[2] = (uint8_t)((mantissa >> 8) & 0xFF);
  result[3] = (uint8_t)((mantissa >> 16) & 0xFF);
  result[4] = (uint8_t)exponent;

  return result;
}

float decodeIEEE11073(const uint8_t *data, uint16_t len) {
  if (len < 5) {
    return std::numeric_limits<float>::quiet_NaN();
  }

  // data[0] is flags. Assuming Celsius.
  int32_t mantissa = (int32_t)(data[1] | (data[2] << 8) | (data[3] << 16));
  if (mantissa == 0x007FFFFF) {
    return std::numeric_limits<float>::quiet_NaN();
  }
  // Handle sign extension for 24-bit signed value
  if (mantissa & 0x800000) {
    mantissa |= 0xFF000000;
  }
  int8_t exponent = (int8_t)data[4];
  return (float)mantissa * pow(10.0f, exponent);
}

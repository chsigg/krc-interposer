#include "sfloat.h"
#include <cmath>

std::array<uint8_t, 5> encodeIEEE11073(float temp) {
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
    return 20.0f;
  }
  // data[0] is flags. Assuming Celsius.
  int32_t mantissa = (int32_t)(data[1] | (data[2] << 8) | (data[3] << 16));
  // Handle sign extension for 24-bit signed value
  if (mantissa & 0x800000) {
    mantissa |= 0xFF000000;
  }
  int8_t exponent = (int8_t)data[4];
  return (float)mantissa * pow(10.0f, exponent);
}

#include "TemperatureClient.h"
#include <math.h>

float TemperatureClient::decodeIEEE11073(const uint8_t* data) {
  // data[0] is flags. Bit 0: 0=Celsius, 1=Fahrenheit.
  // We assume Celsius (0) for simplicity based on prompt "Health Thermometer".
  // Actual float is at data[1]..data[4].

  uint32_t val = (uint32_t)data[1] |
                 ((uint32_t)data[2] << 8) |
                 ((uint32_t)data[3] << 16) |
                 ((uint32_t)data[4] << 24);

  int32_t mantissa = val & 0x00FFFFFF;
  int8_t exponent = (int8_t)(val >> 24);

  // Handle 24-bit signed mantissa
  if (mantissa & 0x00800000) {
    mantissa |= 0xFF000000;
  }

  return (float)mantissa * pow(10, exponent);
}

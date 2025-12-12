#include "BleTemperatureClient.h"
#include "Streaming.h"
#include "Logger.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>

BleTemperatureClient::BleTemperatureClient(StoveSupervisor &supervisor,
                                           TrendAnalyzer &analyzer)
    : BleClient(UUID16_SVC_HEALTH_THERMOMETER,
                UUID16_CHR_INTERMEDIATE_TEMPERATURE),
      supervisor_(supervisor), analyzer_(analyzer) {}

bool BleTemperatureClient::connectCallback(const char *name) {
  for (const char *supported_name : {"DUROMATIC", "HOTPAN", "FAKEPOT"}) {
    if (strcmp(name, supported_name) != 0) {
      continue;
    }

    supervisor_.takeSnapshot();

    return true;
  }

  return false;
}

static float decodeIEEE11073(const uint8_t *data) {
  // data[0] is flags. Bit 0: 0=Celsius, 1=Fahrenheit.
  // We assume Celsius (0) for simplicity based on prompt "Health
  // Thermometer". Actual float is at data[1]..data[4].

  uint32_t val = (uint32_t)data[1] | ((uint32_t)data[2] << 8) |
                 ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24);

  int32_t mantissa = val & 0x00FFFFFF;
  int8_t exponent = (int8_t)(val >> 24);

  // Handle 24-bit signed mantissa
  if (mantissa & 0x00800000) {
    mantissa |= 0xFF000000;
  }

  return static_cast<float>(mantissa) * std::pow(10.0f, exponent);
}

void BleTemperatureClient::notifyCallback(uint8_t *data, uint16_t len) {
  if (len < 5) {
    return; // Flags (1) + Float (4) minimum
  }

  float temp = decodeIEEE11073(data);

  Log << "BleTemperatureClient::notifyCallback(" << temp << "°C)\n";

  analyzer_.addReading(temp, millis());
}

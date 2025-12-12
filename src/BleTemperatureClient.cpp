#include "BleTemperatureClient.h"
#include "Streaming.h"
#include "Logger.h"
#include "sfloat.h"
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

void BleTemperatureClient::notifyCallback(uint8_t *data, uint16_t len) {
  if (len < 5) {
    return; // Flags (1) + Float (4) minimum
  }

  float temp = decodeIEEE11073(data, len);

  Log << "BleTemperatureClient::notifyCallback(" << temp << "°C)\n";

  analyzer_.addReading(temp, millis());
}

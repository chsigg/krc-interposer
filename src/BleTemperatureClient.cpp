#include "BleTemperatureClient.h"
#include "TemperatureClient.h"
#include <Arduino.h>

BleTemperatureClient::BleTemperatureClient(StoveSupervisor& supervisor, TrendAnalyzer& analyzer)
    : BleClient(UUID16_SVC_HEALTH_THERMOMETER,
                UUID16_CHR_INTERMEDIATE_TEMPERATURE),
      supervisor_(supervisor),
      analyzer_(analyzer) {}

void BleTemperatureClient::connectionCallback(bool connected) {
  if (connected) {
    supervisor_.takeSnapshot();
  }
  is_connected_ = connected;
}

void BleTemperatureClient::notifyCallback(uint8_t *data, uint16_t len) {
  if (len < 5) {
    return; // Flags (1) + Float (4) minimum
  }

  last_update_ms_ = millis();
  float temp = TemperatureClient::decodeIEEE11073(data);

  analyzer_.addReading(temp, last_update_ms_);
}

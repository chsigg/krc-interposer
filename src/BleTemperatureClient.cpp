#include "BleTemperatureClient.h"
#include "Streaming.h"
#include <Arduino.h>
#include <algorithm>

BleTemperatureClient::BleTemperatureClient(StoveSupervisor &supervisor,
                                           TrendAnalyzer &analyzer)
    : BleClient(UUID16_SVC_HEALTH_THERMOMETER,
                UUID16_CHR_INTERMEDIATE_TEMPERATURE),
      supervisor_(supervisor), analyzer_(analyzer) {}

bool BleTemperatureClient::scanCallback(
    const ble_gap_evt_adv_report_t *report) {

  if (!report->type.scan_response) {
    if (BleClient::scanCallback(report)) {
      std::copy_n(report->peer_addr.addr, adv_addr_.size(), adv_addr_.begin());
    }
    return false;
  }

  if (!std::equal(adv_addr_.begin(), adv_addr_.end(), report->peer_addr.addr)) {
    return false;
  }

  char name[32] = {};
  Bluefruit.Scanner.parseReportByType(
      report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
      reinterpret_cast<uint8_t *>(name), std::size(name) - 1);

  if (strcmp(name, "DUROMATIC") != 0 && strcmp(name, "HOTPAN") != 0) {
    return false;
  }

  Serial << "Connecting to " << name << " (";
  Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  Serial << ")" << endl;

  return true;
}

void BleTemperatureClient::connectionCallback(bool connected) {
  if (connected) {
    supervisor_.takeSnapshot();
  }
}

static float decodeIEEE11073(const uint8_t *data) {
  // data[0] is flags. Bit 0: 0=Celsius, 1=Fahrenheit.
  // We assume Celsius (0) for simplicity based on prompt "Health Thermometer".
  // Actual float is at data[1]..data[4].

  uint32_t val = (uint32_t)data[1] | ((uint32_t)data[2] << 8) |
                 ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24);

  int32_t mantissa = val & 0x00FFFFFF;
  int8_t exponent = (int8_t)(val >> 24);

  // Handle 24-bit signed mantissa
  if (mantissa & 0x00800000) {
    mantissa |= 0xFF000000;
  }

  return (float)mantissa * pow(10, exponent);
}

void BleTemperatureClient::notifyCallback(uint8_t *data, uint16_t len) {
  if (len < 5) {
    return; // Flags (1) + Float (4) minimum
  }

  float temp = decodeIEEE11073(data);

  Serial << "BleTemperatureClient::notifyCallback(" << temp << "°C)" << endl;

  analyzer_.addReading(temp, millis());
}

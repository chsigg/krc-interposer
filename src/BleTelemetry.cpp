#include "BleTelemetry.h"
#include <Arduino.h>

// Health Thermometer Service
#define BLE_UUID_HEALTH_THERMOMETER_SERVICE "1809"
#define BLE_UUID_TEMPERATURE_MEASUREMENT_CHAR "2A1C"
#define BLE_UUID_INTERMEDIATE_TEMPERATURE_CHAR "2A1E"

BleTelemetry::BleTelemetry(BLEUart &uart, ThermalController &thermalController,
                           TrendAnalyzer &trendAnalyzer)
    : bleuart_(uart), thermal_controller_(thermalController),
      trend_analyzer_(trendAnalyzer),
      service_(BLE_UUID_HEALTH_THERMOMETER_SERVICE),
      temp_measurement_(BLE_UUID_TEMPERATURE_MEASUREMENT_CHAR),
      intermediate_temp_(BLE_UUID_INTERMEDIATE_TEMPERATURE_CHAR),
      last_update_(0) {}

static void initializeCharacteristic(BLECharacteristic &characteristic) {
  characteristic.setProperties(CHR_PROPS_NOTIFY);
  characteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  characteristic.setMaxLen(5); // 1 byte flags + 4 bytes float
  characteristic.begin();
}

void BleTelemetry::begin() {
  bleuart_.bufferTXD(true);
  bleuart_.begin();

  initializeCharacteristic(temp_measurement_);
  initializeCharacteristic(intermediate_temp_);
  service_.begin();

  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart_);
  Bluefruit.Advertising.addService(service_);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // 20ms to 152.5ms
  Bluefruit.Advertising.setFastTimeout(30);   // 30 seconds

  Bluefruit.Advertising.start(0);
}

static std::array<uint8_t, 5> encodeIEEE11073(float temp) {
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

void BleTelemetry::update() {

  if (!Bluefruit.connected()) {
    return;
  }

  bleuart_.flushTXD();

  if (millis() - last_update_ < 1000) {
    return;
  }
  last_update_ = millis();

  auto controller_temp = encodeIEEE11073(thermal_controller_.getTargetTemp());
  temp_measurement_.notify(controller_temp.data(), controller_temp.size());

  auto trend_temp = encodeIEEE11073(trend_analyzer_.getValue(millis()));
  intermediate_temp_.notify(trend_temp.data(), trend_temp.size());
}

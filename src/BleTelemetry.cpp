#include "BleTelemetry.h"
#include "sfloat.h"
#include <Arduino.h>

BleTelemetry::BleTelemetry(BLEUart &blueuart,
                           ThermalController &thermal_controller,
                           const TrendAnalyzer &trend_analyzer)
    : bleuart_(blueuart), thermal_controller_(thermal_controller),
      trend_analyzer_(trend_analyzer) {}

void BleTelemetry::begin() {
  bleuart_.bufferTXD(true);
  bleuart_.begin();

  temp_measurement_.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_WRITE);
  temp_measurement_.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  temp_measurement_.setWriteCallback(tempMeasurementWrittenCallback);
  temp_measurement_.setMaxLen(5); // 1 byte flags + 4 bytes float
  temp_measurement_.begin();

  intermediate_temp_.setProperties(CHR_PROPS_NOTIFY);
  intermediate_temp_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  intermediate_temp_.setMaxLen(5); // 1 byte flags + 4 bytes float
  intermediate_temp_.begin();

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

void BleTelemetry::update() {

  if (!Bluefruit.connected()) {
    return;
  }

  bleuart_.flushTXD();

  if (millis() - last_update_ < 1000) {
    return;
  }
  last_update_ = millis();

  auto controller_temp =
      encodeIEEE11073(thermal_controller_.getTargetTemp());
  temp_measurement_.notify(controller_temp.data(), controller_temp.size());

  auto trend_temp = encodeIEEE11073(trend_analyzer_.getValue(millis()));
  intermediate_temp_.notify(trend_temp.data(), trend_temp.size());
}

void BleTelemetry::tempMeasurementWrittenCallback(uint16_t conn_hdl,
                                                  BLECharacteristic *chr,
                                                  uint8_t *data, uint16_t len) {
  if (len < 5) {
    return; // Flags (1) + Float (4) minimum
  }

  float temp = decodeIEEE11073(data, len);
  static_cast<TempMeasurement *>(chr)
      ->telemetry->thermal_controller_.setTargetTemp(temp);
}

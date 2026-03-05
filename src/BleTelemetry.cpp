#include "BleTelemetry.h"
#include "Logger.h"
#include "sfloat.h"
#include <Arduino.h>
#include <cmath>

static void connectCallback(uint16_t conn_handle) {
  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  if (!conn) {
    Log << "Failed to get connection\n";
    return;
  }

  std::array<char, 32> name = {};
  conn->getPeerName(name.data(), name.size() - 1);
  Log << "BleTelemetry::connectCallback(" << name.data() << ")\n";
}

static void disconnectCallback(uint16_t conn_handle, uint8_t reason) {
  Log << "BleTelemetry::disconnectCallback(/*handle=*/" << conn_handle
      << ", /*reason=*/" << reason << ")\n";
}

BleTelemetry::BleTelemetry(BLEUart &blueuart,
                           ThermalController &thermal_controller,
                           const TrendAnalyzer &trend_analyzer)
    : bleuart_(blueuart), thermal_controller_(thermal_controller),
      trend_analyzer_(trend_analyzer) {}

void BleTelemetry::begin() {
  Bluefruit.Periph.setConnectCallback(connectCallback);
  Bluefruit.Periph.setDisconnectCallback(disconnectCallback);

  bleuart_.bufferTXD(true);
  bleuart_.begin();

  service_.begin();

  target_temp_.setProperties(CHR_PROPS_NOTIFY);
  target_temp_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  target_temp_.setFixedLen(5); // 1 byte flags + 4 bytes float
  target_temp_.begin();

  current_temp_.setProperties(CHR_PROPS_NOTIFY);
  current_temp_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  current_temp_.setFixedLen(5); // 1 byte flags + 4 bytes float
  current_temp_.begin();

  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart_);
  Bluefruit.Advertising.addService(service_);
  Bluefruit.Advertising.restartOnDisconnect(true);
  // Advertise every 320*0.625ms=200ms.
  Bluefruit.Advertising.setInterval(/*fast=*/320, /*slow=*/320);
  Bluefruit.Advertising.start(/*timeout=*/0);
}

void BleTelemetry::end() {
  Bluefruit.Advertising.restartOnDisconnect(false);
  Bluefruit.Advertising.stop();
  uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
  if (Bluefruit.getConnectedHandles(&conn_handle, 1)) {
    Bluefruit.disconnect(conn_handle);
  }
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
  target_temp_.notify(controller_temp.data(), controller_temp.size());

  float current_temp = std::numeric_limits<float>::quiet_NaN();
  if (trend_analyzer_.connected() && trend_analyzer_.getLastUpdateMs() != 0) {
    current_temp = trend_analyzer_.getValue(millis());
  }

  auto encoded_temp = encodeIEEE11073(current_temp);
  current_temp_.notify(encoded_temp.data(), encoded_temp.size());
}

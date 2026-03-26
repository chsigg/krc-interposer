#include "BleTelemetry.h"
#include "Logger.h"
#include "sfloat.h"
#include <Arduino.h>
#include <cmath>

BleTelemetry::BleTelemetry(ThermalController &thermal_controller,
                           const TrendAnalyzer &trend_analyzer,
                           BufferedLogger &buffered_logger)
    : thermal_controller_(thermal_controller), trend_analyzer_(trend_analyzer),
      buffered_logger_(buffered_logger) {}

void BleTelemetry::begin() {
  Bluefruit.Periph.setConnectCallback([](uint16_t conn_handle) {
    Log << "BleTelemetry::connectCallback(/*handle=*/" << conn_handle << ")\n";
  });

  Bluefruit.Periph.setDisconnectCallback(
      [](uint16_t conn_handle, uint8_t reason) {
        Log << "BleTelemetry::disconnectCallback(/*handle=*/" << conn_handle
            << ", /*reason=*/" << reason << ")\n";
      });

  temp_service_.begin();

  target_temp_.setProperties(CHR_PROPS_NOTIFY);
  target_temp_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  target_temp_.setFixedLen(5); // 1 byte flags + 4 bytes float
  target_temp_.begin();

  current_temp_.setProperties(CHR_PROPS_NOTIFY);
  current_temp_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  current_temp_.setFixedLen(5); // 1 byte flags + 4 bytes float
  current_temp_.begin();

  log_service_.begin();

  log_char_.setProperties(CHR_PROPS_INDICATE);
  log_char_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  log_char_.setMaxLen(BLE_GATT_ATT_MTU_MAX);
  log_char_.begin();

  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(temp_service_);
  Bluefruit.Advertising.restartOnDisconnect(true);
  // Advertise every 320*0.625ms=200ms.
  Bluefruit.Advertising.setInterval(/*fast=*/320, /*slow=*/320);
  Bluefruit.Advertising.start(/*timeout=*/0);

  // Create background task for log indications.
  // Stack size 1024 words, Priority Low (2).
  xTaskCreate([](void *p) { static_cast<BleTelemetry *>(p)->logTask(); },
              "BleLog", 1024, this, 2, NULL);
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

  uint32_t now_ms = millis();

  float target_temp = thermal_controller_.getTargetTemp();
  float current_temp = std::numeric_limits<float>::quiet_NaN();
  if (trend_analyzer_.connected()) {
    current_temp = trend_analyzer_.getValue(now_ms);
  }
  bool has_changed = std::fabs(last_target_temp_ - target_temp) > 1.0f ||
                     std::fabs(last_current_temp_ - current_temp) > 1.0f;

  if (now_ms - last_update_ms_ < (has_changed ? 100 : 1000)) {
    return;
  }
  last_update_ms_ = now_ms;
  last_target_temp_ = target_temp;
  last_current_temp_ = current_temp;

  auto target_temp_enc = encodeIEEE11073(target_temp);
  target_temp_.notify(target_temp_enc.data(), target_temp_enc.size());
  auto current_temp_enc = encodeIEEE11073(current_temp);
  current_temp_.notify(current_temp_enc.data(), current_temp_enc.size());
}

void BleTelemetry::logTask() {
  uint8_t buffer[BLE_GATT_ATT_MTU_MAX];

  while (true) {
    size_t available = buffered_logger_.available();
    BLEConnection *conn = Bluefruit.Connection(Bluefruit.connHandle());
    if (available == 0 || conn == nullptr || !log_char_.indicateEnabled()) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    static constexpr size_t kGattHeaderBytes = 3;
    size_t chunk_size = std::min(available, conn->getMtu() - kGattHeaderBytes);

    size_t peeked = buffered_logger_.peek(buffer, chunk_size);
    if (!log_char_.indicate(buffer, peeked)) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    buffered_logger_.consume(peeked);
  }
}

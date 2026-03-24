#pragma once

#include <array>
#include <bluefruit.h>
#include <cstdint>
#include <limits>

#include "ArduinoDigitalWritePin.h"
#include "Blinker.h"
#include "TrendAnalyzer.h"

class BleThermometer {
public:
  using Address = std::array<uint8_t, BLE_GAP_ADDR_LEN>;

  BleThermometer(TrendAnalyzer &analyzer);
  ~BleThermometer();

  void begin();
  void end();
  void update();

private:
  void scanCallback(ble_gap_evt_adv_report_t *report);
  void connectCallback(uint16_t conn_handle);
  void disconnectCallback(uint16_t conn_handle, uint8_t reason);
  void notifyCallback(uint8_t *data, uint16_t len);

  TrendAnalyzer &analyzer_;

  ArduinoDigitalWritePin blue_led_{LED_BLUE};
  Blinker blue_blinker_{blue_led_};

  BLEClientService service_ = {UUID16_SVC_HEALTH_THERMOMETER};
  BLEClientCharacteristic char_intermediate_ = {
      UUID16_CHR_INTERMEDIATE_TEMPERATURE};
  BLEClientCharacteristic char_measurement_ = {
      UUID16_CHR_TEMPERATURE_MEASUREMENT};

  std::array<Address, 8> allow_addresses_ = {};
  size_t num_allow_addresses_ = 0;

  uint32_t last_data_ms_ = std::numeric_limits<int32_t>::max();
  uint32_t last_rssi_read_ms_ = std::numeric_limits<int32_t>::max();
};

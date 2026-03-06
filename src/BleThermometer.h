#pragma once

#include <array>
#include <bluefruit.h>
#include <cstdint>
#include <limits>

#include "ArduinoDigitalWritePin.h"
#include "Blinker.h"
#include "Thermometer.h"
#include "TrendAnalyzer.h"

class BleThermometer : public Thermometer {

  class IntermediateTemp final : public BLEClientCharacteristic {
  public:
    IntermediateTemp(BleThermometer *client)
        : BLEClientCharacteristic(UUID16_CHR_INTERMEDIATE_TEMPERATURE),
          client(client) {}
    BleThermometer *client;
  };

public:
  BleThermometer(TrendAnalyzer &analyzer);
  ~BleThermometer();

  void begin();
  void end();
  void update() override;

private:
  enum class State {
    IDLE,
    CONNECTING,
    CONNECTED,
    DISCOVERING_SERVICE,
    DISCOVERING_CHAR,
    ENABLING_NOTIFY,
    ONLINE
  };

  void transitionTo(State new_state);
  const char *getStateName(State state) const;

  bool connectCallback(const char *name);
  void notifyCallback(uint8_t *data, uint16_t len);

  static void globalScanCallback(ble_gap_evt_adv_report_t *report);
  static void globalConnectCallback(uint16_t conn_handle);
  static void globalDisconnectCallback(uint16_t conn_handle, uint8_t reason);
  static void globalNotifyCallback(BLEClientCharacteristic *chr, uint8_t *data,
                                   uint16_t len);

  TrendAnalyzer &analyzer_;

  ArduinoDigitalWritePin blue_led_{LED_BLUE};
  Blinker blue_blinker_{blue_led_};

  BLEClientService service_ = {UUID16_SVC_HEALTH_THERMOMETER};
  IntermediateTemp char_ = {this};
  uint32_t last_rssi_read_ms_ = 0;
  uint32_t last_data_ms_ = std::numeric_limits<int32_t>::max();

  State state_ = State::IDLE;
  uint32_t state_entry_ms_ = 0;
  uint16_t conn_handle_ = BLE_CONN_HANDLE_INVALID;
};

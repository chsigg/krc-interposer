#pragma once

#include "StoveSupervisor.h"
#include "TrendAnalyzer.h"
#include <array>
#include <bluefruit.h>
#include <cstdint>

class BleThermometer {

  class IntermediateTemp final : public BLEClientCharacteristic {
  public:
    IntermediateTemp(BleThermometer *client)
        : BLEClientCharacteristic(UUID16_CHR_INTERMEDIATE_TEMPERATURE), client(client) {}
    BleThermometer *client;
  };

public:
  BleThermometer(StoveSupervisor &supervisor, TrendAnalyzer &analyzer);
  ~BleThermometer();

  void begin();
  void start();
  void stop();
  bool connected();

private:
  bool connectCallback(const char *name);
  void notifyCallback(uint8_t *data, uint16_t len);

  static void globalConnectCallback(uint16_t conn_handle);
  static void globalDisconnectCallback(uint16_t conn_handle, uint8_t reason);
  static void globalScanCallback(ble_gap_evt_adv_report_t *report);
  static void globalNotifyCallback(BLEClientCharacteristic *chr, uint8_t *data,
                                   uint16_t len);

  StoveSupervisor &supervisor_;
  TrendAnalyzer &analyzer_;

  BLEClientService service_= {UUID16_SVC_HEALTH_THERMOMETER};
  IntermediateTemp char_  = {this};
};

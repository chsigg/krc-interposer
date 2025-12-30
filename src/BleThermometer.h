#pragma once

#include "Thermometer.h"
#include "TrendAnalyzer.h"
#include <array>
#include <bluefruit.h>
#include <cstdint>

class BleThermometer : public Thermometer {

  class IntermediateTemp final : public BLEClientCharacteristic {
  public:
    IntermediateTemp(BleThermometer *client)
        : BLEClientCharacteristic(UUID16_CHR_INTERMEDIATE_TEMPERATURE), client(client) {}
    BleThermometer *client;
  };

public:
  BleThermometer(TrendAnalyzer &analyzer);
  ~BleThermometer();

  void begin();
  void start() override;
  void stop() override;
  bool connected() override;

private:
  bool connectCallback(const char *name);
  void notifyCallback(uint8_t *data, uint16_t len);

  static void globalConnectCallback(uint16_t conn_handle);
  static void globalScanCallback(ble_gap_evt_adv_report_t *report);
  static void globalNotifyCallback(BLEClientCharacteristic *chr, uint8_t *data,
                                   uint16_t len);

  TrendAnalyzer &analyzer_;

  BLEClientService service_= {UUID16_SVC_HEALTH_THERMOMETER};
  IntermediateTemp char_  = {this};
};

#pragma once

#include "BleClient.h"
#include "StoveSupervisor.h"
#include "TrendAnalyzer.h"

class BleTemperatureClient : public BleClient {
public:
  BleTemperatureClient(StoveSupervisor& supervisor, TrendAnalyzer& analyzer);

  bool scanCallback(const ble_gap_evt_adv_report_t *report) override;
  void connectionCallback(bool connected) override;
  void notifyCallback(uint8_t* data, uint16_t len) override;

private:
  StoveSupervisor& supervisor_;
  TrendAnalyzer& analyzer_;
  std::array<uint8_t, 6> adv_addr_ = {};
};

#pragma once

#include "BleClient.h"
#include "TemperatureClient.h"
#include "StoveSupervisor.h"
#include "TrendAnalyzer.h"

class BleTemperatureClient : public BleClient, public TemperatureClient {
public:
  BleTemperatureClient(StoveSupervisor& supervisor, TrendAnalyzer& analyzer);

  bool isConnected() const override { return is_connected_; }

  uint32_t getLastUpdateMs() const override { return last_update_ms_; }

  void connectionCallback(bool connected) override;
  void notifyCallback(uint8_t* data, uint16_t len) override;

private:
  StoveSupervisor& supervisor_;
  TrendAnalyzer& analyzer_;
  uint32_t last_update_ms_ = 0;
  bool is_connected_ = false;
};

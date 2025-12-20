#pragma once

#include "BleClient.h"
#include "StoveSupervisor.h"
#include "TrendAnalyzer.h"

class BleTemperatureClient final : public BleClient {
public:
  BleTemperatureClient(StoveSupervisor& supervisor, TrendAnalyzer& analyzer);

  bool connectCallback(const char* name) override;
  void notifyCallback(uint8_t* data, uint16_t len) override;

private:
  StoveSupervisor& supervisor_;
  TrendAnalyzer& analyzer_;
};

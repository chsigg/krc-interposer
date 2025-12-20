#pragma once

#include "BleClient.h"
#include "StoveSupervisor.h"

class BleShutterClient final : public BleClient {
public:
  BleShutterClient(StoveSupervisor& supervisor);

  void notifyCallback(uint8_t* data, uint16_t len) override;

private:
  StoveSupervisor& supervisor_;
};

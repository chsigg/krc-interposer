#include "BleShutterClient.h"

BleShutterClient::BleShutterClient(StoveSupervisor& supervisor)
    : BleClient(UUID16_SVC_HUMAN_INTERFACE_DEVICE, UUID16_CHR_REPORT),
      supervisor_(supervisor) {}

void BleShutterClient::notifyCallback(uint8_t *data, uint16_t len) {
  supervisor_.takeSnapshot();
}

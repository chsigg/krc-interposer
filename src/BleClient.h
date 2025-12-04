#pragma once

#include <bluefruit.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <sys/types.h>

class BleClient {
public:
  BleClient(uint16_t service_uuid, uint16_t char_uuid);
  virtual ~BleClient();

  virtual bool scanCallback(const std::string &name) { return true; };
  virtual void connectionCallback(bool connected){};
  virtual void notifyCallback(uint8_t *data, uint16_t len){};

  static void begin();

private:
  static void connectCallback(uint16_t conn_handle);
  static void disconnectCallback(uint16_t conn_handle, uint8_t reason);
  static void scanCallback(ble_gap_evt_adv_report_t *report);
  static void notifyCallback(BLEClientCharacteristic *chr, uint8_t *data,
                             uint16_t len);

  BLEClientService service_;
  std::unique_ptr<BLEClientCharacteristic> char_;
};

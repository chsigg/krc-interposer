#pragma once

#include <array>
#include <bluefruit.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <sys/types.h>
#include <vector>

class BleClient {
public:
  BleClient(uint16_t service_uuid, uint16_t char_uuid);
  virtual ~BleClient();

  virtual bool connectCallback(const char* name) { return true; };
  virtual void notifyCallback(uint8_t *data, uint16_t len){};

  static void begin();
  static void update(bool enabled);

private:
  static void startScan();
  static void stopScanAndDisconnect();
  static void globalConnectCallback(uint16_t conn_handle);
  static void globalDisconnectCallback(uint16_t conn_handle, uint8_t reason);
  static void globalScanCallback(ble_gap_evt_adv_report_t *report);
  static void globalNotifyCallback(BLEClientCharacteristic *chr, uint8_t *data,
                                   uint16_t len);

  class BleCharacteristic : public BLEClientCharacteristic {
  public:
    BleCharacteristic(uint16_t uuid, BleClient *client)
        : BLEClientCharacteristic(uuid), client(client) {}
    BleClient *client;
  };

  BLEClientService service_;
  BleCharacteristic char_;
};

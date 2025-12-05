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

  bool isConnected() const;

  virtual bool connectCallback(const char* name) { return true; };
  virtual void disconnectCallback(uint8_t reason) {}
  virtual void notifyCallback(uint8_t *data, uint16_t len){};

  static void begin();

private:
  static void startScan();
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

#include "BleClient.h"
#include "Streaming.h"
#include <Arduino.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

static std::array<BleClient *, 2> sBleClients = {};

namespace {

class BleCharacteristic : public BLEClientCharacteristic {
public:
  BleCharacteristic(uint16_t uuid, BleClient *client)
      : BLEClientCharacteristic(uuid), client(client) {}
  BleClient *client;
};

} // namespace

BleClient::BleClient(uint16_t service_uuid, uint16_t char_uuid)
    : service_(service_uuid),
      char_(std::make_unique<BleCharacteristic>(char_uuid, this)) {
  for (auto &client : sBleClients) {
    if (client == nullptr) {
      client = this;
      return;
    }
  }
  assert(false && "Too many BleClients");
}

BleClient::~BleClient() {
  for (auto &client : sBleClients) {
    if (client == this) {
      client = nullptr;
      break;
    }
  }
}

void BleClient::begin() {
  Serial << "BleClient::begin()" << endl;

  Bluefruit.begin(0, sBleClients.size());
  Bluefruit.setName("KRC Interposer");

  // Callbacks
  Bluefruit.Central.setConnectCallback(connectCallback);
  Bluefruit.Central.setDisconnectCallback(disconnectCallback);
  // Bluefruit.Central.setConnInterval(8, 1600); // 10ms - 2s

  std::array<BLEUuid, sBleClients.size()> uuids;
  uint8_t count = 0;

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }
    client->service_.begin();
    client->char_->setNotifyCallback(notifyCallback);
    client->char_->begin(&client->service_);
    uuids[count++] = client->service_.uuid;
  }

  Bluefruit.Scanner.setRxCallback(scanCallback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80); // Scan every 200ms for 100ms
  Bluefruit.Scanner.useActiveScan(true);
  Bluefruit.Scanner.filterUuid(uuids.data(), count);
  Bluefruit.Scanner.start(0);
}

void BleClient::scanCallback(ble_gap_evt_adv_report_t *report) {

  char name[32] = {};
  auto get_name = [&](uint8_t type) {
    return Bluefruit.Scanner.parseReportByType(
        report, type, reinterpret_cast<uint8_t *>(name), std::size(name) - 1);
  };
  get_name(BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME) ||
      get_name(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME);

  Serial << "BleClient::scanCallback(" << name << ")" << endl;

  auto is_disconnected = [](BleClient *client) {
    return client && client->service_.connHandle() == BLE_CONN_HANDLE_INVALID;
  };
  size_t num_disconnected =
      std::count_if(sBleClients.begin(), sBleClients.end(), is_disconnected);
  if (num_disconnected == 0) {
    return;
  }

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }

    if (!Bluefruit.Scanner.checkReportForService(report, client->service_)) {
      continue;
    }

    if (!client->scanCallback(name)) {
      continue;
    }

    Serial << "  Connecting " << client->service_.uuid.toString() << endl;
    Bluefruit.Central.connect(report);

    if (num_disconnected > 1) {
      Bluefruit.Scanner.resume();
    }

    return;
  }

  Bluefruit.Scanner.resume();
}

void BleClient::connectCallback(uint16_t conn_handle) {
  Serial << "BleClient::connectCallback(/*handle=*/" << conn_handle << ")"
         << endl;

  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  if (!conn) {
    return;
  }

  // interval = 1000ms, latency = 0, timeout = 5000ms
  if (!conn->requestConnectionParameter(800, 0, 500)) {
    Serial << "Failed to request connection parameters" << endl;
  }

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }

    if (!client->service_.discover(conn_handle)) {
      continue;
    }

    if (!client->char_->discover()) {
      Serial << "Failed to discover characteristic" << endl;
      // TODO: disconnect
      continue;
    }

    client->char_->enableNotify();
    client->connectionCallback(true);
    Serial << "  Connected " << client->service_.uuid.toString() << endl;
    return;
  }
}

void BleClient::disconnectCallback(uint16_t conn_handle, uint8_t reason) {
  Serial << "BleClient::disconnectCallback(/*handle=*/" << conn_handle
         << ", /*reason=*/" << reason << ")" << endl;

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }

    if (client->service_.connHandle() != conn_handle) {
      continue;
    }

    client->connectionCallback(false);
    return;
  }
}

void BleClient::notifyCallback(BLEClientCharacteristic *characteristic,
                               uint8_t *data, uint16_t len) {
  static_cast<BleCharacteristic *>(characteristic)
      ->client->notifyCallback(data, len);
}

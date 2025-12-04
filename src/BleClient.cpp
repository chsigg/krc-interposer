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

bool BleClient::scanCallback(const ble_gap_evt_adv_report_t *report) {
  return Bluefruit.Scanner.checkReportForService(report, service_);
};

void BleClient::begin() {
  Serial << "BleClient::begin()" << endl;

  Bluefruit.begin(0, sBleClients.size());
  Bluefruit.setName("KRC Interposer");

  Bluefruit.Central.setConnectCallback(globalConnectCallback);
  Bluefruit.Central.setDisconnectCallback(globalDisconnectCallback);

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }
    client->service_.begin();
    client->char_->setNotifyCallback(globalNotifyCallback);
    client->char_->begin(&client->service_);
  }

  Bluefruit.Scanner.setRxCallback(globalScanCallback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80); // Scan every 200ms for 100ms
  Bluefruit.Scanner.useActiveScan(true);
  Bluefruit.Scanner.start(0);
}

void BleClient::globalScanCallback(ble_gap_evt_adv_report_t *report) {

  Serial << "BleClient::scanCallback(";
  Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  Serial << ")" << endl;

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

    if (!client->scanCallback(report)) {
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

void BleClient::globalConnectCallback(uint16_t conn_handle) {
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

void BleClient::globalDisconnectCallback(uint16_t conn_handle, uint8_t reason) {
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

void BleClient::globalNotifyCallback(BLEClientCharacteristic *characteristic,
                               uint8_t *data, uint16_t len) {
  static_cast<BleCharacteristic *>(characteristic)
      ->client->notifyCallback(data, len);
}

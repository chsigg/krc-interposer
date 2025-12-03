#include "BleClient.h"
#include <Arduino.h>
#include <cassert>
#include <cstddef>
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
  bool found = false;
  for (auto &client : sBleClients) {
    if (client == nullptr) {
      client = this;
      found = true;
      break;
    }
  }
  assert(found && "Too many BleClients");

  [[maybe_unused]] static bool initialized = [] {
    return initialize(), true;
  }();
  service_.begin();
  char_->setNotifyCallback(notifyCallback);
  char_->begin(&service_);
}

BleClient::~BleClient() {
  for (auto &client : sBleClients) {
    if (client == this) {
      client = nullptr;
      break;
    }
  }
}

void BleClient::initialize() {
  Bluefruit.begin(0, sBleClients.size());
  Bluefruit.setName("KRC Interposer");

  // Callbacks
  Bluefruit.Central.setConnectCallback(connectCallback);
  Bluefruit.Central.setDisconnectCallback(disconnectCallback);
  Bluefruit.Scanner.setRxCallback(scanCallback);

  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80); // 100ms
  Bluefruit.Scanner.useActiveScan(true);
  Bluefruit.Scanner.start(0);
}

void BleClient::scanCallback(ble_gap_evt_adv_report_t *report) {
  Serial.print("BleClient Scan Callback: ");

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }

    if (!Bluefruit.Scanner.checkReportForUuid(report, client->service_.uuid)) {
      continue;
    }

    Serial.print("Connecting to ");
    Serial.println(client->service_.uuid.toString());
    Bluefruit.Central.connect(report);
    return;
  }
}

Serial.println("no client found");
}

void BleClient::connectCallback(uint16_t conn_handle) {
  Serial.print("BleClient Connect Callback: handle ");
  Serial.println(conn_handle);

  if (BLEConnection *conn = Bluefruit.Connection(conn_handle); !conn) {
    return;
  }

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }

    if (!client->service_.discover(conn_handle)) {
      continue;
    }

    if (!client->char_->discover()) {
      Serial.println("BleClient: Service discovered but Char discovery failed");
      // TODO: disconnect
      continue;
    }

    client->char_->enableNotify();
    client->connectionCallback(true);
    Serial.println("BleClient: Connected and subscribed");
    return;
  }
}

void BleClient::disconnectCallback(uint16_t conn_handle, uint8_t reason) {
  Serial.print("BleClient Disconnect Callback: ");
  Serial.print(conn_handle);
  Serial.print(" Reason: ");
  Serial.println(reason);
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

#include "BleClient.h"
#include "Logger.h"
#include <Arduino.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

static std::array<BleClient *, 2> sBleClients = {};
static std::vector<std::array<uint8_t, 6>> sDenyList;

BleClient::BleClient(uint16_t service_uuid, uint16_t char_uuid)
    : service_(service_uuid), char_(char_uuid, this) {
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

bool BleClient::isConnected() const {
  return const_cast<BLEClientService &>(service_).connHandle() !=
         BLE_CONN_HANDLE_INVALID;
}

void BleClient::begin() {
  Log << "BleClient::begin()\n";

  Bluefruit.begin(1, sBleClients.size());
  Bluefruit.setName("KRC Interposer");

  Bluefruit.Central.setConnectCallback(globalConnectCallback);
  Bluefruit.Central.setDisconnectCallback(globalDisconnectCallback);

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }
    client->service_.begin();
    client->char_.setNotifyCallback(globalNotifyCallback);
    client->char_.begin(&client->service_);
  }

  Bluefruit.Scanner.setRxCallback(globalScanCallback);
  Bluefruit.Scanner.setInterval(160, 80); // Scan every 200ms for 100ms
  Bluefruit.Scanner.restartOnDisconnect(false);
  Bluefruit.Scanner.useActiveScan(false);
  startScan();
}

void BleClient::startScan() {
  Log << "BleClient::startScan()\n";

  uint8_t count = 0;
  std::array<BLEUuid, sBleClients.size()> uuids;
  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }
    if (client->isConnected()) {
      continue;
    }
    uuids[count++] = client->service_.uuid;
  }
  if (count == 0) {
    return;
  }

  Bluefruit.Scanner.filterUuid(uuids.data(), count);
  Bluefruit.Scanner.start(0);
}

void BleClient::globalScanCallback(ble_gap_evt_adv_report_t *report) {

  for (const auto &addr : sDenyList) {
    if (memcmp(report->peer_addr.addr, addr.data(), addr.size()) == 0) {
      Bluefruit.Scanner.resume();
      return;
    }
  }

  Log << "BleClient::globalScanCallback(";
  for (int i = 5; i >= 0; --i) {
    char hex[3] = {};
    uint8_t byte = report->peer_addr.addr[i];
    hex[0] = "0123456789ABCDEF"[byte >> 4];
    hex[1] = "0123456789ABCDEF"[byte & 0x0F];
    Log << hex;
    if (i > 0) {
      Log << ":";
    }
  }
  Log << ")\n";

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }

    if (!Bluefruit.Scanner.checkReportForService(report, client->service_)) {
      continue;
    }

    if (client->isConnected()) {
      Log << "  Already connected\n";
      continue;
    }

    Log << "  Connecting to 0x" << client->service_.uuid.toString().c_str()
        << "\n";
    Bluefruit.Central.connect(report);

    return;
  }

  Log << "  Resuming scanner\n";
  Bluefruit.Scanner.resume();
}

void BleClient::globalConnectCallback(uint16_t conn_handle) {

  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  if (!conn) {
    Log << "Failed to get connection\n";
    return;
  }

  std::array<char, 32> name = {};
  conn->getPeerName(name.data(), name.size() - 1);
  Log << "BleClient::globalConnectCallback(" << name.data() << ")\n";

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }

    if (!client->service_.discover(conn_handle)) {
      continue;
    }

    if (!client->connectCallback(name.data())) {
      Log << "  Refused to connect\n";
      break;
    }

    if (!client->char_.discover()) {
      Log << "  Failed to discover characteristic\n";
      break;
    }

    if (!client->char_.enableNotify()) {
      Log << "  Failed to enable notifications\n";
      break;
    }

    Log << "  Connected\n";
    startScan();
    return;
  }

  std::array<uint8_t, 6> addr;
  std::copy_n(conn->getPeerAddr().addr, addr.size(), addr.begin());
  sDenyList.push_back(addr);
  conn->disconnect();
}

void BleClient::globalDisconnectCallback(uint16_t conn_handle, uint8_t reason) {
  Log << "BleClient::globalDisconnectCallback(/*handle=*/" << conn_handle
      << ", /*reason=*/" << reason << ")\n";

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }

    if (client->service_.connHandle() != conn_handle) {
      continue;
    }

    client->disconnectCallback(reason);
    break;
  }

  startScan();
}

void BleClient::globalNotifyCallback(BLEClientCharacteristic *characteristic,
                                     uint8_t *data, uint16_t len) {
  static_cast<BleCharacteristic *>(characteristic)
      ->client->notifyCallback(data, len);
}

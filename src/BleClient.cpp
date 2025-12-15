#include "BleClient.h"
#include "Logger.h"
#include <Arduino.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

static std::array<BleClient *, 2> sBleClients = {};
static std::vector<std::array<uint8_t, BLE_GAP_ADDR_LEN>> sDenyList;
static bool sEnabled = false;

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
}

void BleClient::update(bool enabled) {
  if (sEnabled == enabled) {
    return;
  }
  sEnabled = enabled;
  if (sEnabled) {
    startScan();
  } else {
    stopScanAndDisconnect();
  }
}

void BleClient::startScan() {
  Log << "BleClient::startScan()\n";

  uint8_t count = 0;
  std::array<BLEUuid, sBleClients.size()> uuids;
  for (auto client : sBleClients) {
    if (client == nullptr || client->service_.discovered()) {
      continue;
    }
    uuids[count++] = client->service_.uuid;
  }
  if (count == 0) {
    return;
  }

  if (Bluefruit.Scanner.isRunning()) {
    Bluefruit.Scanner.stop();
  }
  Bluefruit.Scanner.filterUuid(uuids.data(), count);
  Bluefruit.Scanner.start(0);
}

void BleClient::stopScanAndDisconnect() {
  Log << "BleClient::stop()\n";
  Bluefruit.Scanner.stop();
  for (auto client : sBleClients) {
    if (client == nullptr || !client->service_.discovered()) {
      continue;
    }
    if (BLEConnection *conn =
            Bluefruit.Connection(client->service_.connHandle())) {
      conn->disconnect();
    }
  }
}

static void logAddress(const uint8_t *addr) {
  for (const uint8_t *it = addr + BLE_GAP_ADDR_LEN; it-- > addr;) {
    char hex[3] = {};
    hex[0] = "0123456789ABCDEF"[*it >> 4];
    hex[1] = "0123456789ABCDEF"[*it & 0x0F];
    Log << hex;
    if (it != addr) {
      Log << ":";
    }
  }
}

void BleClient::globalScanCallback(ble_gap_evt_adv_report_t *report) {

  for (const auto &addr : sDenyList) {
    if (memcmp(report->peer_addr.addr, addr.data(), addr.size()) == 0) {
      Bluefruit.Scanner.resume();
      return;
    }
  }

  Log << "BleClient::globalScanCallback(";
  logAddress(report->peer_addr.addr);
  Log << ")\n";

  for (auto client : sBleClients) {
    if (client == nullptr) {
      continue;
    }

    if (!Bluefruit.Scanner.checkReportForService(report, client->service_)) {
      continue;
    }

    if (client->service_.discovered()) {
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
    if (client == nullptr || client->service_.discovered()) {
      continue;
    }

    if (!client->service_.discover(conn_handle)) {
      continue;
    }

    if (!client->connectCallback(name.data())) {
      std::array<uint8_t, BLE_GAP_ADDR_LEN> addr;
      std::copy_n(conn->getPeerAddr().addr, addr.size(), addr.begin());
      sDenyList.push_back(addr);
      Log << "  Refused to connect, added ";
      logAddress(addr.data());
      Log << " to deny list\n";
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
    return startScan();
  }

  conn->disconnect();
}

void BleClient::globalDisconnectCallback(uint16_t conn_handle, uint8_t reason) {
  Log << "BleClient::globalDisconnectCallback(/*handle=*/" << conn_handle
      << ", /*reason=*/" << reason << ")\n";

  if (sEnabled) {
    startScan();
  }
}

void BleClient::globalNotifyCallback(BLEClientCharacteristic *characteristic,
                                     uint8_t *data, uint16_t len) {
  static_cast<BleCharacteristic *>(characteristic)
      ->client->notifyCallback(data, len);
}

#include "BleClient.h"
#include "Logger.h"
#include <Arduino.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

static std::array<BleClient *, 2> sBleClients = {};

struct DeniedClient {
  std::array<uint8_t, BLE_GAP_ADDR_LEN> addr;
  uint32_t deny_until_ms;
};

static std::array<DeniedClient, 16> sDenyList;
static size_t sDenyListCount = 0;

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

bool BleClient::connected() { return service_.discovered(); }

void BleClient::start() {
  Log << "BleClient::start()\n";

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

  Bluefruit.Central.setDisconnectCallback(globalDisconnectCallback);

  if (Bluefruit.Scanner.isRunning()) {
    Bluefruit.Scanner.stop();
  }
  Bluefruit.Scanner.filterUuid(uuids.data(), count);
  Bluefruit.Scanner.start(0);
}

void BleClient::stop() {
  Log << "BleClient::stop()\n";

  Bluefruit.Central.setDisconnectCallback(nullptr);

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

static void addDeniedClient(const uint8_t *address, uint32_t timeout_ms) {
  std::array<uint8_t, BLE_GAP_ADDR_LEN> addr;
  std::copy_n(address, BLE_GAP_ADDR_LEN, addr.begin());
  uint32_t deny_until_ms = millis() + timeout_ms;

  auto begin = sDenyList.begin();
  auto end = begin + sDenyListCount;
  auto it = std::find_if(begin, end, [&](const DeniedClient &client) {
    return client.addr == addr;
  });

  if (it == end && end == sDenyList.end()) {
    it = begin;
  }

  if (it != end) {
    std::move(it + 1, end, it);
    --sDenyListCount;
  }

  sDenyList[sDenyListCount++] = {addr, deny_until_ms};
}

static void trimDenyList() {
  uint32_t now = millis();
  auto begin = sDenyList.begin();
  auto end = std::remove_if(
      begin, begin + sDenyListCount, [&](const DeniedClient &client) {
        return client.deny_until_ms - now > std::numeric_limits<int32_t>::max();
      });
  sDenyListCount = std::distance(begin, end);
}

void BleClient::globalScanCallback(ble_gap_evt_adv_report_t *report) {
  std::array<uint8_t, BLE_GAP_ADDR_LEN> addr;
  std::copy_n(report->peer_addr.addr, BLE_GAP_ADDR_LEN, addr.begin());

  Log << "BleClient::globalScanCallback(";
  logAddress(addr.data());
  Log << ")\n";

  trimDenyList();
  for (size_t i = 0; i < sDenyListCount; ++i) {
    if (sDenyList[i].addr != addr) {
      continue;
    }
    Log << "  Denied\n";
    Bluefruit.Scanner.resume();
    return;
  }

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
    addDeniedClient(report->peer_addr.addr, 10 * 1000);
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
      addDeniedClient(conn->getPeerAddr().addr, 10 * 60 * 1000);
      Log << "  Refused to connect, added ";
      logAddress(conn->getPeerAddr().addr);
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
    return start();
  }

  conn->disconnect();
}

void BleClient::globalDisconnectCallback(uint16_t conn_handle, uint8_t reason) {
  Log << "BleClient::globalDisconnectCallback(/*handle=*/" << conn_handle
      << ", /*reason=*/" << reason << ")\n";

  start();
}

void BleClient::globalNotifyCallback(BLEClientCharacteristic *characteristic,
                                     uint8_t *data, uint16_t len) {
  static_cast<BleCharacteristic *>(characteristic)
      ->client->notifyCallback(data, len);
}

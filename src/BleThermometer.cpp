#include "BleThermometer.h"
#include "Logger.h"
#include "sfloat.h"
#include <Arduino.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

static BleThermometer *sBleThermometer = nullptr;

struct DeniedClient {
  std::array<uint8_t, BLE_GAP_ADDR_LEN> addr;
  uint32_t deny_until_ms;
};

static std::array<DeniedClient, 16> sDenyList;
static size_t sDenyListCount = 0;

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

BleThermometer::BleThermometer(TrendAnalyzer &analyzer) : analyzer_(analyzer) {
  assert(sBleThermometer == nullptr && "Too many BleThermometers");
  sBleThermometer = this;
}

BleThermometer::~BleThermometer() {
  if (sBleThermometer == this) {
    sBleThermometer = nullptr;
  }
}

static void globalDisconnectCallback(uint16_t conn_handle, uint8_t reason) {
  Log << "globalDisconnectCallback(/*handle=*/" << conn_handle
      << ", /*reason=*/" << reason << ")\n";
}

void BleThermometer::begin() {
  Log << "BleThermometer::begin()\n";

  Bluefruit.begin(1, 1);
  Bluefruit.setName("KRC Interposer");

  Bluefruit.Central.setConnectCallback(globalConnectCallback);
  Bluefruit.Central.setDisconnectCallback(globalDisconnectCallback);

  service_.begin();
  char_.setNotifyCallback(globalNotifyCallback);
  char_.begin(&service_);

  Bluefruit.Scanner.setRxCallback(globalScanCallback);
  Bluefruit.Scanner.setInterval(160, 80); // Scan every 200ms for 100ms
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.filterUuid(service_.uuid);
}

bool BleThermometer::connected() { return service_.discovered(); }

void BleThermometer::start() {
  Log << "BleThermometer::start()\n";
  if (service_.discovered()) {
    return;
  }
  if (Bluefruit.Scanner.isRunning()) {
    Bluefruit.Scanner.stop();
  }
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.start(0);
}

void BleThermometer::stop() {
  Log << "BleThermometer::stop()\n";

  Bluefruit.Scanner.restartOnDisconnect(false);
  Bluefruit.Scanner.stop();
  if (service_.discovered()) {
    if (BLEConnection *conn = Bluefruit.Connection(service_.connHandle())) {
      conn->disconnect();
    }
  }
}

bool BleThermometer::connectCallback(const char *name) {
  Log << "BleThermometer::connectCallback(" << name << ")\n";

  for (const char *supported_name : {"DUROMATIC", "HOTPAN", "FAKEPOT"}) {
    if (strcmp(name, supported_name) != 0) {
      continue;
    }
    return true;
  }

  return false;
}

void BleThermometer::notifyCallback(uint8_t *data, uint16_t len) {
  if (len < 5) {
    return; // Flags (1) + Float (4) minimum
  }

  float temp = decodeIEEE11073(data, len);

  Log << "BleThermometer::notifyCallback(" << temp << "°C)\n";

  analyzer_.addReading(temp, millis());
}

void BleThermometer::globalScanCallback(ble_gap_evt_adv_report_t *report) {
  std::array<uint8_t, BLE_GAP_ADDR_LEN> addr;
  std::copy_n(report->peer_addr.addr, BLE_GAP_ADDR_LEN, addr.begin());

  Log << "BleThermometer::globalScanCallback(";
  logAddress(addr.data());
  Log << ")\n";

  std::unique_ptr<BLEScanner, void (*)(BLEScanner *)> resumer(
      &Bluefruit.Scanner, [](BLEScanner *scanner) {
        Log << "  Resuming scanner\n";
        scanner->resume();
      });

  trimDenyList();
  for (size_t i = 0; i < sDenyListCount; ++i) {
    if (sDenyList[i].addr != addr) {
      continue;
    }
    Log << "  Denied\n";
    return;
  }

  if (!sBleThermometer || !Bluefruit.Scanner.checkReportForService(
                              report, sBleThermometer->service_)) {
    return;
  }

  if (sBleThermometer->service_.discovered()) {
    Log << "  Already connected\n";
    return;
  }

  resumer.release();
  Log << "  Connecting to 0x"
      << sBleThermometer->service_.uuid.toString().c_str() << "\n";
  addDeniedClient(report->peer_addr.addr, 10 * 1000);
  Bluefruit.Central.connect(report);
}

void BleThermometer::globalConnectCallback(uint16_t conn_handle) {

  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  if (!conn) {
    Log << "Failed to get connection\n";
    return;
  }

  std::array<char, 32> name = {};
  conn->getPeerName(name.data(), name.size() - 1);
  Log << "BleThermometer::globalConnectCallback(" << name.data() << ")\n";

  std::unique_ptr<BLEConnection, void (*)(BLEConnection *)> disconnector(
      conn, [](BLEConnection *connection) { connection->disconnect(); });

  if (!sBleThermometer) {
    return;
  }

  if (sBleThermometer->service_.discovered()) {
    Log << "  Service already discovered\n";
    return;
  }

  if (!sBleThermometer->service_.discover(conn_handle)) {
    Log << "  Service discovery failed\n";
    return;
  }

  if (!sBleThermometer->connectCallback(name.data())) {
    addDeniedClient(conn->getPeerAddr().addr, 10 * 60 * 1000);
    Log << "  Refused to connect, added ";
    logAddress(conn->getPeerAddr().addr);
    Log << " to deny list\n";
    return;
  }

  if (!sBleThermometer->char_.discover()) {
    Log << "  Failed to discover characteristic\n";
    return;
  }

  if (!sBleThermometer->char_.enableNotify()) {
    Log << "  Failed to enable notifications\n";
    return;
  }

  disconnector.release();
  Log << "  Connected\n";
}

void BleThermometer::globalNotifyCallback(
    BLEClientCharacteristic *characteristic, uint8_t *data, uint16_t len) {
  static_cast<IntermediateTemp *>(characteristic)
      ->client->notifyCallback(data, len);
}

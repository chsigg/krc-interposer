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
  uint32_t now_ms = millis();
  auto begin = sDenyList.begin();
  auto end = std::remove_if(begin, begin + sDenyListCount,
                            [&](const DeniedClient &client) {
                              return client.deny_until_ms - now_ms >
                                     std::numeric_limits<int32_t>::max();
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

void BleThermometer::begin() {
  Log << "BleThermometer::begin()\n";

  Bluefruit.Central.setConnectCallback(globalConnectCallback);
  Bluefruit.Central.setDisconnectCallback(globalDisconnectCallback);

  service_.begin();
  char_.setNotifyCallback(globalNotifyCallback);
  char_.begin(&service_);

  Bluefruit.Scanner.setRxCallback(globalScanCallback);
  // Scan every 160*0.625ms=100ms for 40*0.624ms=25ms.
  Bluefruit.Scanner.setInterval(160, 40);
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.filterUuid(service_.uuid);
  Bluefruit.Scanner.restartOnDisconnect(false);
  Bluefruit.Scanner.start(0);

  blue_led_.begin();
  blue_blinker_.blink(Blinker::Signal::REPEAT);
}

void BleThermometer::end() {
  Bluefruit.Scanner.stop();
  if (conn_handle_ != BLE_CONN_HANDLE_INVALID) {
    Bluefruit.disconnect(conn_handle_);
  }
  transitionTo(State::IDLE);
}

void BleThermometer::update() {
  blue_blinker_.update();
  analyzer_.setConnected(millis() - last_data_ms_ < 30 * 1000);

  uint32_t now_ms = millis();
  uint32_t elapsed_ms = now_ms - state_entry_ms_;

  switch (state_) {
  case State::IDLE:
    if (!Bluefruit.Scanner.isRunning()) {
      Log << "Resuming scanner\n";
      Bluefruit.Scanner.start(0);
    }
    break;

  case State::CONNECTING:
    if (elapsed_ms > 10000) {
      Log << "Connection timeout\n";
      transitionTo(State::IDLE);
    }
    break;

  case State::CONNECTED:
    if (elapsed_ms <= 500) {
      break;
    }
    Log << "Discovering service...\n";
    transitionTo(State::DISCOVERING_SERVICE);
    if (service_.discover(conn_handle_)) {
      transitionTo(State::DISCOVERING_CHAR);
    } else {
      Log << "Service discovery failed\n";
      Bluefruit.disconnect(conn_handle_);
    }
    break;

  case State::DISCOVERING_SERVICE:
    break;

  case State::DISCOVERING_CHAR:
    if (elapsed_ms <= 200) {
      break;
    }
    Log << "Discovering characteristic...\n";
    if (char_.discover()) {
      transitionTo(State::ENABLING_NOTIFY);
    } else {
      Log << "Char discovery failed\n";
      Bluefruit.disconnect(conn_handle_);
    }
    break;

  case State::ENABLING_NOTIFY:
    if (elapsed_ms <= 200) {
      break;
    }
    Log << "Enabling notifications...\n";
    if (char_.enableNotify()) {
      transitionTo(State::ONLINE);
      if (BLEConnection *conn = Bluefruit.Connection(conn_handle_)) {
        conn->monitorRssi();
      }
    } else {
      Log << "Enable notify failed\n";
      Bluefruit.disconnect(conn_handle_);
    }
    break;

  case State::ONLINE:
    if (now_ms - last_rssi_read_ms_ > 10000) {
      if (BLEConnection *conn = Bluefruit.Connection(conn_handle_)) {
        Log << "RSSI: " << conn->getRssi() << "dBm\n";
        last_rssi_read_ms_ = now_ms;
      }
    }
    break;
  }
}

void BleThermometer::transitionTo(State new_state) {
  if (state_ == new_state) {
    return;
  }

  Log << "BleThermometer: " << getStateName(state_) << " -> "
      << getStateName(new_state) << "\n";

  state_ = new_state;
  state_entry_ms_ = millis();

  switch (state_) {
  case State::IDLE:
    conn_handle_ = BLE_CONN_HANDLE_INVALID;
    blue_blinker_.blink(Blinker::Signal::REPEAT);
    break;
  case State::ONLINE:
    blue_blinker_.blink(Blinker::Signal::SOLID);
    break;
  default:
    break;
  }
}

const char *BleThermometer::getStateName(State state) const {
  switch (state) {
  case State::IDLE:
    return "IDLE";
  case State::CONNECTING:
    return "CONNECTING";
  case State::CONNECTED:
    return "CONNECTED";
  case State::DISCOVERING_SERVICE:
    return "DISCOVERING_SERVICE";
  case State::DISCOVERING_CHAR:
    return "DISCOVERING_CHAR";
  case State::ENABLING_NOTIFY:
    return "ENABLING_NOTIFY";
  case State::ONLINE:
    return "ONLINE";
  }
  return "UNKNOWN";
}

bool BleThermometer::connectCallback(const char *name) {
  Log << "BleThermometer::connectCallback(" << name << ")\n";

  for (const char *supported_name : {"DUROMATIC", "HOTPAN", "FAKEPOT"}) {
    if (strcmp(name, supported_name) == 0) {
      return true;
    }
  }

  return false;
}

void BleThermometer::notifyCallback(uint8_t *data, uint16_t len) {
  if (len < 5) {
    return; // Flags (1) + Float (4) minimum
  }

  float temp = decodeIEEE11073(data, len);
  Log << "BleThermometer::notifyCallback(" << temp << "°C)\n";

  uint32_t now_ms = millis();
  last_data_ms_ = now_ms;
  analyzer_.addReading(temp, now_ms);
}

void BleThermometer::globalScanCallback(ble_gap_evt_adv_report_t *report) {
  if (!sBleThermometer || sBleThermometer->state_ != State::IDLE) {
    return;
  }

  std::array<uint8_t, BLE_GAP_ADDR_LEN> addr;
  std::copy_n(report->peer_addr.addr, BLE_GAP_ADDR_LEN, addr.begin());

  trimDenyList();
  for (size_t i = 0; i < sDenyListCount; ++i) {
    if (sDenyList[i].addr == addr) {
      return;
    }
  }

  if (!Bluefruit.Scanner.checkReportForService(report,
                                               sBleThermometer->service_)) {
    return;
  }

  Log << "Found lid: ";
  logAddress(addr.data());
  Log << "\n";

  Bluefruit.Scanner.stop();
  sBleThermometer->transitionTo(State::CONNECTING);
  Bluefruit.Central.connect(report);
}

void BleThermometer::globalConnectCallback(uint16_t conn_handle) {
  Log << "BleThermometer::globalConnectCallback(" << conn_handle << ")\n";

  if (!sBleThermometer || sBleThermometer->state_ != State::CONNECTING) {
    Bluefruit.disconnect(conn_handle);
    return;
  }

  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  if (!conn) {
    sBleThermometer->transitionTo(State::IDLE);
    return;
  }

  std::array<char, 32> name = {};
  conn->getPeerName(name.data(), name.size() - 1);

  if (!sBleThermometer->connectCallback(name.data())) {
    addDeniedClient(conn->getPeerAddr().addr, 10 * 60 * 1000);
    Bluefruit.disconnect(conn_handle);
    return;
  }

  sBleThermometer->conn_handle_ = conn_handle;
  sBleThermometer->transitionTo(State::CONNECTED);
}

void BleThermometer::globalDisconnectCallback(uint16_t conn_handle,
                                              uint8_t reason) {
  Log << "globalDisconnectCallback(/*handle=*/" << conn_handle
      << ", /*reason=*/" << reason << ")\n";

  if (!sBleThermometer || sBleThermometer->conn_handle_ != conn_handle) {
    return;
  }

  if (BLEConnection *conn = Bluefruit.Connection(conn_handle)) {
    addDeniedClient(conn->getPeerAddr().addr, 5000);
  }
  sBleThermometer->transitionTo(State::IDLE);
}

void BleThermometer::globalNotifyCallback(
    BLEClientCharacteristic *characteristic, uint8_t *data, uint16_t len) {
  static_cast<IntermediateTemp *>(characteristic)
      ->client->notifyCallback(data, len);
}

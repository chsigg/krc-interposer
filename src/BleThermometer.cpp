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

inline Logger &
operator<<(Logger &logger,
           const std::array<uint8_t, BLE_GAP_ADDR_LEN> &address) {
  constexpr char kHex[] = "0123456789ABCDEF";
  for (auto it = address.rbegin();;) {
    logger << std::array<char, 3>{kHex[*it >> 4], kHex[*it & 0x0F], '\0'}.data();
    if (++it == address.rend()) {
      break;
    }
    logger << ":";
  }
  return logger;
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

  Bluefruit.Central.setConnectCallback([](uint16_t conn_handle) {
    if (sBleThermometer) {
      sBleThermometer->connectCallback(conn_handle);
    }
  });

  Bluefruit.Central.setDisconnectCallback(
      [](uint16_t conn_handle, uint8_t reason) {
        if (sBleThermometer) {
          sBleThermometer->disconnectCallback(conn_handle, reason);
        }
      });

  service_.begin();

  auto notify_cb = [](BLEClientCharacteristic *, uint8_t *data, uint16_t len) {
    if (sBleThermometer) {
      sBleThermometer->notifyCallback(data, len);
    }
  };

  char_intermediate_.setNotifyCallback(notify_cb);
  char_intermediate_.begin(&service_);

  char_measurement_.setIndicateCallback(notify_cb);
  char_measurement_.begin(&service_);

  Bluefruit.Scanner.setRxCallback([](ble_gap_evt_adv_report_t *report) {
    if (sBleThermometer) {
      sBleThermometer->scanCallback(report);
    }
  });

  // Scan continuously (100% duty cycle): 160*0.625ms = 100ms
  Bluefruit.Scanner.setInterval(160, 160);
  Bluefruit.Scanner.useActiveScan(true);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.start(0);

  blue_led_.begin();
  blue_blinker_.blink(Blinker::Signal::REPEAT);
}

void BleThermometer::end() {
  Bluefruit.Scanner.restartOnDisconnect(false);
  Bluefruit.Scanner.stop();
  Bluefruit.disconnect(service_.connHandle());
  blue_blinker_.blink(Blinker::Signal::NONE);
}

void BleThermometer::update() {
  blue_blinker_.update();

  uint32_t now_ms = millis();
  BLEConnection *conn = Bluefruit.Connection(service_.connHandle());

  analyzer_.setConnected(conn && now_ms - last_data_ms_ < 30 * 1000);

  if (conn && now_ms - last_rssi_read_ms_ >= 10 * 1000) {
    Log << "BleThermometer: RSSI = " << conn->getRssi() << "dBm\n";
    last_rssi_read_ms_ = now_ms;
  }
}

void BleThermometer::scanCallback(ble_gap_evt_adv_report_t *report) {
  std::unique_ptr<BLEScanner, void (*)(BLEScanner *)> resumer(
      &Bluefruit.Scanner, [](BLEScanner *scanner) { scanner->resume(); });

  Address address;
  std::copy_n(report->peer_addr.addr, address.size(), address.begin());

  auto begin = allow_addresses_.begin();
  auto end = begin + num_allow_addresses_;
  auto it = std::find(begin, end, address);

  if (it == end) {
    if (!Bluefruit.Scanner.checkReportForService(report, service_)) {
      return;
    }

    if (end == allow_addresses_.end()) {
      std::move(begin + 1, end--, begin);
    } else {
      ++num_allow_addresses_;
    }
    *end = address;
    Log << "BleThermometer: Added " << address << " to allow-list\n";
  }

  std::array<char, 32> name = {};
  if (!Bluefruit.Scanner.parseReportByType(
          report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
          reinterpret_cast<uint8_t *>(name.data()), name.size() - 1) ||
      strlen(name.data()) == 0) {
    Log << "BleThermometer: No name in advertising report\n";
    return;
  }

  static constexpr const char *kNames[] = {"DUROMATIC", "HOTPAN", "FAKEPOT"};
  auto pred = [&](const char *supported) {
    return strcmp(name.data(), supported) == 0;
  };
  if (std::none_of(std::begin(kNames), std::end(kNames), pred)) {
    Log << "BleThermometer: Unrecognized name: '" << name.data() << "'\n";
    return;
  }

  Log << "BleThermometer: Found " << name.data() << " at " << address << "\n";
  resumer.release();
  Bluefruit.Central.connect(report);
}

void BleThermometer::connectCallback(uint16_t conn_handle) {
  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  if (!conn) {
    Log << "BleThermometer: Failed to get connection object\n";
    return;
  }

  // Match phone app's delay before GATT discovery
  delay(100);

  std::unique_ptr<BLEConnection, void (*)(BLEConnection *)> disconnector(
      conn, [](BLEConnection *conn) { conn->disconnect(); });

  if (!service_.discover(conn_handle)) {
    Log << "BleThermometer: Service discovery failed\n";
    return;
  }

  if (!char_intermediate_.discover()) {
    Log << "BleThermometer: Intermediate temperature discovery failed\n";
    return;
  }

  if (!char_measurement_.discover()) {
    Log << "BleThermometer: Temperature measurement discovery failed\n";
    return;
  }

  if (!char_intermediate_.enableNotify()) {
    Log << "BleThermometer: Enable notify failed\n";
    return;
  }

  // Request slow connection (440ms interval, 20s timeout)
  if (!conn->requestConnectionParameter(352, 0, 2000)) {
    Log << "BleThermometer: Failed to request connection parameters\n";
    return;
  }

  if (!conn->monitorRssi()) {
    Log << "BleThermometer: Enable RSSI monitor failed\n";
    return;
  }

  disconnector.release();
  blue_blinker_.blink(Blinker::Signal::SOLID);
  Log << "BleThermometer: Connected and online\n";
}

void BleThermometer::disconnectCallback(uint16_t conn_handle, uint8_t reason) {
  Log << "BleThermometer::disconnectCallback(" << conn_handle << ", " << reason
      << ")\n";

  blue_blinker_.blink(Blinker::Signal::REPEAT);
}

void BleThermometer::notifyCallback(uint8_t *data, uint16_t len) {
  if (len < 5) {
    return; // Flags (1) + Float (4) minimum
  }

  float temp = decodeIEEE11073(data, len);
  last_data_ms_ = millis();
  analyzer_.addReading(temp, last_data_ms_);
}

#pragma once
#include <cstdint>
#include <functional>

// Abstract interface for the Temperature Source (Thermometer)
class TemperatureClient {
public:
  virtual ~TemperatureClient() = default;

  // Should return true if currently connected to a device
  virtual bool isConnected() const = 0;

  // Returns the timestamp (ms) of the last received valid data
  virtual uint32_t getLastUpdateMs() const = 0;

protected:
  static float decodeIEEE11073(const uint8_t *data);
};

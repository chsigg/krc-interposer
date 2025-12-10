#pragma once

#include <Print.h>
#include <cstdint>

// This class tees output to two Print classes. It inherits from Print to
// support all the standard printing and streaming functions.
class TeePrint : public Print {
public:
  // Constructor takes references to the two streams.
  TeePrint(Print &primary, Print &secondary)
      : primary_(primary), secondary_(secondary) {}

  // Core write method - sends a single byte to the primary stream and buffers
  // it for the secondary stream.
  size_t write(uint8_t c) override {
    primary_.write(c);
    secondary_.write(c);
    return 1;
  }

  // Overload for writing a buffer.
  size_t write(const uint8_t *buffer, size_t size) override {
    primary_.write(buffer, size);
    secondary_.write(buffer, size);
    return size;
  }

private:
  Print &primary_;
  Print &secondary_;
};

extern TeePrint Log;

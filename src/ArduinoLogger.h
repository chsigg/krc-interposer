#pragma once

#include "Logger.h"
#include <algorithm>
#include <Print.h>

// This class tees output to two Print classes.
class ArduinoLogger final : public Logger {
public:
  // Constructor takes references to the two streams.
  ArduinoLogger(Print &primary, Print &secondary)
      : primary_(primary), secondary_(secondary) {}

  void log(const char* msg, size_t length) override {
    while (length > 0) {
      size_t chunk_size = std::min<size_t>(length,20);
      primary_.write(msg, chunk_size);
      secondary_.write(msg, chunk_size);
      msg += chunk_size;
      length -= chunk_size;
    }
  }

  void log(long val) override {
    primary_.print(val);
    secondary_.print(val);
  }
  void log(unsigned long val) override {
    primary_.print(val);
    secondary_.print(val);
  }
  void log(float val) override {
    primary_.print(val);
    secondary_.print(val);
  }

private:
  Print &primary_;
  Print &secondary_;
};

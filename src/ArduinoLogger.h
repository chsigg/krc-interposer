#pragma once

#include "Logger.h"
#include <Print.h>

// This class tees output to two Print classes.
class ArduinoLogger final : public Logger {
public:
  // Constructor takes references to the two streams.
  ArduinoLogger(Print &primary, Print &secondary)
      : primary_(primary), secondary_(secondary) {}

  void log(const char* msg, size_t length) override {
    primary_.write(msg, length);
    secondary_.write(msg, length);
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

#pragma once

#include "Logger.h"
#include "BufferedLogger.h"
#include <Arduino.h>

// This class tees output to Serial and another Logger.
class ArduinoLogger final : public Logger {
public:
  explicit ArduinoLogger(Logger &logger) : logger_(logger) {}

  void log(const char* msg, size_t length) override {
    Serial.write(msg, length);
    logger_.log(msg, length);
  }

  void log(long val) override {
    Serial.print(val);
    logger_.log(val);
  }

  void log(unsigned long val) override {
    Serial.print(val);
    logger_.log(val);
  }

  void log(float val) override {
    Serial.print(val);
    logger_.log(val);
  }

private:
  Logger &logger_;
};

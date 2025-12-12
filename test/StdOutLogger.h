#pragma once

#include "Logger.h"
#include <iostream>
#include <string>

class StdOutLogger : public Logger {
public:
  void log(const char* msg, size_t length) override {
    std::cout.write(msg, length);
  }
  void log(long val) override {
    std::cout << val;
  }
  void log(unsigned long val) override {
    std::cout << val;
  }
  void log(float val) override {
    std::cout << val;
  }
};

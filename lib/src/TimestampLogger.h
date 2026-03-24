#pragma once

#include "Logger.h"

class TimestampLogger final : public Logger {
public:
  explicit TimestampLogger(Logger &target) : target_(target) {}

  void log(const char *msg, size_t length) override;
  void log(long val) override;
  void log(unsigned long val) override;
  void log(float val) override;

private:
  void maybePrintTimestamp();

  Logger &target_;
  bool at_start_of_line_ = true;
};

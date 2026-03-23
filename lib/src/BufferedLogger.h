#pragma once

#include "Logger.h"
#include <atomic>
#include <cstdint>
#include <vector>
#include <cstddef>

class BufferedLogger : public Logger {
public:
  explicit BufferedLogger(size_t size);

  // Logger interface
  void log(const char *msg, size_t length) override;
  void log(long val) override;
  void log(unsigned long val) override;
  void log(float val) override;

  // Consumer interface
  size_t available() const;
  size_t peek(uint8_t *buffer, size_t max_len) const;
  void consume(size_t len);

private:
  size_t write(const uint8_t *buffer, size_t size);

  std::vector<uint8_t> buffer_;
  std::atomic<size_t> head_{0}; // Producer index
  std::atomic<size_t> tail_{0}; // Consumer index
};

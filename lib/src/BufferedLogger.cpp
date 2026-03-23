#include "BufferedLogger.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

BufferedLogger::BufferedLogger(size_t size) : buffer_(size) {
  assert(size > 0 && (size & (size - 1)) == 0 && "Size must be power of two");
}

void BufferedLogger::log(const char *msg, size_t length) {
  write(reinterpret_cast<const uint8_t *>(msg), length);
}

void BufferedLogger::log(long val) {
  char buf[32];
  int len = std::snprintf(buf, sizeof(buf), "%ld", val);
  if (len > 0) {
    log(buf, static_cast<size_t>(len));
  }
}

void BufferedLogger::log(unsigned long val) {
  char buf[32];
  int len = std::snprintf(buf, sizeof(buf), "%lu", val);
  if (len > 0) {
    log(buf, static_cast<size_t>(len));
  }
}

void BufferedLogger::log(float val) {
  char buf[32];
  int len = std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(val));
  if (len > 0) {
    log(buf, static_cast<size_t>(len));
  }
}

size_t BufferedLogger::available() const {
  size_t h = head_.load(std::memory_order_acquire);
  size_t t = tail_.load(std::memory_order_relaxed);
  return h - t;
}

size_t BufferedLogger::peek(uint8_t *buffer, size_t max_len) const {
  size_t h = head_.load(std::memory_order_acquire);
  size_t t = tail_.load(std::memory_order_relaxed);
  size_t count = std::min(max_len, h - t);
  if (count == 0) return 0;

  size_t capacity = buffer_.size();
  size_t mask = capacity - 1;
  size_t offset = t & mask;

  size_t first_chunk = std::min(count, capacity - offset);
  std::memcpy(buffer, &buffer_[offset], first_chunk);

  if (count > first_chunk) {
    std::memcpy(buffer + first_chunk, &buffer_[0], count - first_chunk);
  }
  return count;
}

void BufferedLogger::consume(size_t len) {
  size_t t = tail_.load(std::memory_order_relaxed);
  tail_.store(t + len, std::memory_order_release);
}

size_t BufferedLogger::write(const uint8_t *buffer, size_t size) {
  size_t h = head_.load(std::memory_order_relaxed);
  size_t t = tail_.load(std::memory_order_acquire);

  size_t capacity = buffer_.size();
  size_t available_space = capacity - (h - t);
  size_t to_write = std::min(size, available_space);
  if (to_write == 0) return 0;

  size_t mask = capacity - 1;
  size_t offset = h & mask;
  size_t first_chunk = std::min(to_write, capacity - offset);

  std::memcpy(&buffer_[offset], buffer, first_chunk);

  if (to_write > first_chunk) {
    std::memcpy(&buffer_[0], buffer + first_chunk, to_write - first_chunk);
  }

  head_.store(h + to_write, std::memory_order_release);
  return to_write;
}

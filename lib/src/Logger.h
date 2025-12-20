#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <string>

class Logger {
public:
  virtual ~Logger() = default;

  virtual void log(const char *msg, size_t length) = 0;
  virtual void log(long) = 0;
  virtual void log(unsigned long) = 0;
  virtual void log(float) = 0;
};

inline Logger &operator<<(Logger &logger, const char *msg) {
  return logger.log(msg, strlen(msg)), logger;
};

template <size_t N> Logger &operator<<(Logger &logger, const char (&msg)[N]) {
  return logger.log(msg, N - 1), logger;
}

template <typename T,
          typename = std::enable_if_t<std::is_integral_v<T>>>
Logger &operator<<(Logger &logger, T value) {
  if constexpr (std::is_signed_v<T>) {
    return logger.log(static_cast<long>(value)), logger;
  } else {
    return logger.log(static_cast<unsigned long>(value)), logger;
  }
}

inline Logger &operator<<(Logger &logger, float value) {
  return logger.log(value), logger;
}

inline Logger &operator<<(Logger &logger, bool value) {
  return logger << (value ? "true" : "false");
}

extern Logger &Log;

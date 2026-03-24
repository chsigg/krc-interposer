#include "TimestampLogger.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

extern "C" uint32_t millis();

void TimestampLogger::log(const char *msg, size_t length) {
  for (const char *end = msg + length; msg != end;) {
    maybePrintTimestamp();
    const char *it = std::find(msg, end, '\n');
    at_start_of_line_ = it != end;
    it += at_start_of_line_;
    target_.log(msg, it - msg);
    msg = it;
  }
}

void TimestampLogger::log(long val) {
  maybePrintTimestamp();
  target_.log(val);
}

void TimestampLogger::log(unsigned long val) {
  maybePrintTimestamp();
  target_.log(val);
}

void TimestampLogger::log(float val) {
  maybePrintTimestamp();
  target_.log(val);
}

void TimestampLogger::maybePrintTimestamp() {
  if (!at_start_of_line_) {
    return;
  }

  char buf[32];
  uint32_t now_ms = millis();
  uint32_t sec = now_ms / 1000;
  uint32_t centi = (now_ms % 1000) / 10;
  int len = std::snprintf(buf, sizeof(buf), "[%4u.%02u] ", sec, centi);
  if (len > 0) {
    target_.log(buf, static_cast<size_t>(len));
  }
  at_start_of_line_ = false;
}

#pragma once

#include <array>
#include <cstdint>

class TrendAnalyzer {
  struct Reading {
    float value;
    uint32_t time;
  };

public:
  void addReading(float value, uint32_t time);

  float getValue(uint32_t time) const {
    return (time - history_[0].time) * slope_ + intercept_;
  }

  float getSlope() const { return slope_; }

  virtual uint32_t getLastUpdateMs() const {
    return count_ > 0 ? history_[count_ - 1].time : 0;
  }

private:
  void calculateRegression();

  std::array<Reading, 15> history_;
  std::size_t count_ = 0;

  float intercept_ = 0.0f;
  float slope_ = 0.0f;
};

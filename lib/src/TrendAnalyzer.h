#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

// Thread-safe for one writer and multiple readers.
class TrendAnalyzer {
  struct Reading {
    float value;
    uint32_t time_ms;
  };

  struct AnalysisResult {
    uint32_t last_update_ms = 0;
    float intercept = 0.0f;
    float slope = 0.0f;
  };

public:
  virtual ~TrendAnalyzer() = default;

  virtual void addReading(float value, uint32_t time_ms);
  virtual void clear();

  virtual float getValue(uint32_t time) const {
    const auto &result = getAnalysisResult();
    return (time - result.last_update_ms) * result.slope + result.intercept;
  }

  virtual float getSlope() const { return getAnalysisResult().slope; }

  virtual uint32_t getLastUpdateMs() const {
    return getAnalysisResult().last_update_ms;
  }

private:
  const AnalysisResult &getAnalysisResult() const {
    return results_[current_result_index_.load(std::memory_order_acquire)];
  }

  AnalysisResult calculateRegression(size_t count) const;

  std::array<Reading, 15> history_;
  std::atomic<size_t> count_ = 0;

  std::array<AnalysisResult, 2> results_;
  std::atomic<int> current_result_index_{0};
};

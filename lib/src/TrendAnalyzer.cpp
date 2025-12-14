#include "TrendAnalyzer.h"
#include "Logger.h"
#include "sfloat.h"
#include <algorithm>
#include <cmath>
#include <limits>

void TrendAnalyzer::addReading(float value, uint32_t time_ms) {
  Log << "TrendAnalyzer::addReading(/*value=*/" << value << ", /*time_ms=*/"
      << time_ms << "\n";

  Reading reading = {value, time_ms};
  auto begin = history_.begin();
  auto end = begin + count_;
  auto comp = [](const Reading &a, const Reading &b) {
    return static_cast<int32_t>(a.time - b.time) > 0;
  };
  auto it = std::upper_bound(begin, end, reading, comp);

  if (count_ < history_.size()) {
    ++end;
    ++count_;
  }

  if (it == end) {
    return;
  }

  std::move_backward(it, end - 1, end);
  *it = reading;

  const int next_idx =
      1 - current_result_index_.load(std::memory_order_relaxed);
  results_[next_idx] = calculateRegression();
  current_result_index_.store(next_idx, std::memory_order_release);
}

TrendAnalyzer::AnalysisResult TrendAnalyzer::calculateRegression() const {
  uint32_t last_update_ms = history_[0].time;

  if (count_ < 2 || last_update_ms == history_[count_ - 1].time) {
    return {last_update_ms, history_[0].value, 0.0f};
  }

  float sum_x = 0.0f;
  float sum_y = 0.0f;
  float sum_xy = 0.0f;
  float sum_xx = 0.0f;

  for (size_t i = 0; i < count_; ++i) {
    float x = static_cast<int32_t>(history_[i].time - last_update_ms);
    float y = history_[i].value;

    sum_x += x;
    sum_y += y;
    sum_xy += x * y;
    sum_xx += x * x;
  }

  float denominator = count_ * sum_xx - sum_x * sum_x;
  if (std::abs(denominator) < std::numeric_limits<float>::epsilon()) {
    return {last_update_ms, sum_y / count_, 0.0f};
  }

  float slope = (count_ * sum_xy - sum_x * sum_y) / denominator;
  float intercept = (sum_y - slope * sum_x) / count_;
  return {last_update_ms, intercept, slope};
}


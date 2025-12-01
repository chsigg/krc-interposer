#include "TrendAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <limits>

void TrendAnalyzer::addReading(float value, uint32_t time) {
  Reading reading = {value, time};
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

  calculateRegression();
}

void TrendAnalyzer::calculateRegression() {
  if (history_[0].time == history_[count_ - 1].time) {
    slope_ = 0.0f;
    intercept_ = history_[0].value;
    return;
  }

  float sum_x = 0.0f;
  float sum_y = 0.0f;
  float sum_xy = 0.0f;
  float sum_xx = 0.0f;

  for (size_t i = 0; i < count_; ++i) {
    float x = static_cast<int32_t>(history_[i].time - history_[0].time);
    float y = history_[i].value;

    sum_x += x;
    sum_y += y;
    sum_xy += x * y;
    sum_xx += x * x;
  }

  float denominator = count_ * sum_xx - sum_x * sum_x;
  slope_ = (count_ * sum_xy - sum_x * sum_y) / denominator;
  intercept_ = (sum_y - slope_ * sum_x) / count_;
}

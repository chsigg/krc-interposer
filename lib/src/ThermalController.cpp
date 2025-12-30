#include "ThermalController.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

extern "C" uint32_t millis();

ThermalController::ThermalController(const TrendAnalyzer &analyzer,
                                     const ThermalConfig &config)
    : analyzer_(analyzer), config_(config), target_temp_(config.ambient_temp) {}

float ThermalController::getTargetTemp() const {
  return target_temp_.load(std::memory_order_relaxed);
}

void ThermalController::setTargetTemp(float temp) {
  Log << "ThermalController::setTargetTemp(" << temp << ")\n";
  target_temp_.store(temp, std::memory_order_relaxed);
}

void ThermalController::update() {
  uint32_t current_time_ms = millis();
  float slope = analyzer_.getSlope();

  if (slope < -config_.lid_open_threshold) {
    lid_open_ = true;
  }

  if (lid_open_) {
    // Until slope is positive, ignore sensor values and freeze output.
    if (slope < config_.lid_open_threshold) {
      return;
    }
    Log << "ThermalController lid closed\n";
    lid_open_ = false;
  }

  uint32_t future_time = current_time_ms + config_.system_lag_ms;
  float predicted_temp = analyzer_.getValue(future_time);

  float error = target_temp_ - predicted_temp;
  float p_out = error * config_.p_factor;

  float current_temp = analyzer_.getValue(current_time_ms);
  float loss = (current_temp - config_.ambient_temp) * config_.heat_loss_factor;

  power_ = std::clamp(p_out + loss, 0.0f, 1.0f);
}

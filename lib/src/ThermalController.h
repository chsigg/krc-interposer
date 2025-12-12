#pragma once
#include "TrendAnalyzer.h"
#include <cstdint>

struct ThermalConfig {
  float p_factor = 0.1f;               // P-factor (1/K)
  float heat_loss_factor = 0.01f;      // Heat loss factor (1/K)
  uint32_t system_lag_ms = 10000;      // Lookahead time (ms)
  float lid_open_threshold = 0.0005f;  // Threshold for lid open (°C/ms)
  float ambient_temp = 20.0f;          // Ambient temperature (°C)
};

class ThermalController {
public:
  ThermalController(const TrendAnalyzer &analyzer, const ThermalConfig &config);

  void update();

  virtual float getTargetTemp() const { return target_temp_; }
  virtual void setTargetTemp(float temp) { target_temp_ = temp; }
  virtual float getLevel() const { return level_; }
  bool isLidOpen() const { return lid_open_; }

private:
  const TrendAnalyzer &analyzer_;
  const ThermalConfig config_;
  float target_temp_;
  float level_ = 0.0f;
  bool lid_open_ = false;
};

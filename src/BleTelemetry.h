#ifndef BLETELEMETRY_H_
#define BLETELEMETRY_H_

#include "BufferedLogger.h"
#include "ThermalController.h"
#include "TrendAnalyzer.h"
#include <bluefruit.h>
#include <limits>

class BleTelemetry final {

public:
  BleTelemetry(ThermalController &thermalController,
               const TrendAnalyzer &trendAnalyzer,
               BufferedLogger &bufferedLogger);
  void begin();
  void end();
  void update();

private:
  void logTask();

  ThermalController &thermal_controller_;
  const TrendAnalyzer &trend_analyzer_;
  BufferedLogger &buffered_logger_;

  BLEService temp_service_ = {UUID16_SVC_HEALTH_THERMOMETER};
  BLECharacteristic target_temp_ = {UUID16_CHR_TEMPERATURE_MEASUREMENT};
  BLECharacteristic current_temp_ = {UUID16_CHR_INTERMEDIATE_TEMPERATURE};

  BLEService log_service_ = BLEService("EF680001-9B35-4933-9B10-592919114064");
  BLECharacteristic log_char_ = BLECharacteristic("EF680002-9B35-4933-9B10-592919114064");

  uint32_t last_update_ms_ = std::numeric_limits<int32_t>::max();
};

#endif // BLETELEMETRY_H_

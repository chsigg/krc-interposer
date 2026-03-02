#ifndef BLETELEMETRY_H_
#define BLETELEMETRY_H_

#include "ThermalController.h"
#include "TrendAnalyzer.h"
#include <bluefruit.h>

class BleTelemetry final {

public:
  BleTelemetry(BLEUart &bleuart, ThermalController &thermalController,
               const TrendAnalyzer &trendAnalyzer);
  void begin();
  void end();
  void update();

private:
  BLEUart &bleuart_;
  ThermalController &thermal_controller_;
  const TrendAnalyzer &trend_analyzer_;

  BLEService service_ = {UUID16_SVC_HEALTH_THERMOMETER};
  BLECharacteristic target_temp_ = {UUID16_CHR_TEMPERATURE_MEASUREMENT};
  BLECharacteristic current_temp_ = {UUID16_CHR_INTERMEDIATE_TEMPERATURE};

  uint32_t last_update_ = 0;
};

#endif // BLETELEMETRY_H_

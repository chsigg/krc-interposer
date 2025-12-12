#ifndef BLETELEMETRY_H_
#define BLETELEMETRY_H_

#include <bluefruit.h>
#include "ThermalController.h"
#include "TrendAnalyzer.h"

class BleTelemetry {
 public:
  BleTelemetry(BLEUart& uart, ThermalController& thermalController, TrendAnalyzer& trendAnalyzer);
  void begin();
  void update();

 private:
  BLEUart& bleuart_;
  ThermalController& thermal_controller_;
  TrendAnalyzer& trend_analyzer_;

  BLEService service_;
  BLECharacteristic temp_measurement_;
  BLECharacteristic intermediate_temp_;

  unsigned long last_update_;
};

#endif  // BLETELEMETRY_H_

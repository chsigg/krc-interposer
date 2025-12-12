#ifndef BLETELEMETRY_H_
#define BLETELEMETRY_H_

#include "ThermalController.h"
#include "TrendAnalyzer.h"
#include <bluefruit.h>

class BleTelemetry {

  class TempMeasurement : public BLECharacteristic {
  public:
    TempMeasurement(BleTelemetry *telemetry)
        : BLECharacteristic(UUID16_CHR_TEMPERATURE_MEASUREMENT),
          telemetry(telemetry) {}
    BleTelemetry *telemetry;
  };

public:
  BleTelemetry(BLEUart &bleuart, ThermalController &thermalController,
               const TrendAnalyzer &trendAnalyzer);
  void begin();
  void update();

private:
  static void tempMeasurementWrittenCallback(uint16_t conn_hdl, BLECharacteristic *chr,
                                      uint8_t *data, uint16_t len);

  BLEUart &bleuart_;
  ThermalController &thermal_controller_;
  const TrendAnalyzer &trend_analyzer_;

  BLEService service_ = {UUID16_SVC_HEALTH_THERMOMETER};
  TempMeasurement target_temp_ = {this};
  BLECharacteristic current_temp_ = {UUID16_CHR_INTERMEDIATE_TEMPERATURE};

  uint32_t last_update_ = 0;
};

#endif // BLETELEMETRY_H_

#pragma once

#include "AnalogReadPin.h"
#include "StoveLevel.h"
#include <array>
#include <cstdint>

class StoveDial {
public:
  StoveDial(const AnalogReadPin &pin, const StoveLevelConfig &config);

  void update();

  StoveLevel getLevel() const;

private:
  const AnalogReadPin &pin_;
  const StoveLevelConfig config_;

  StoveLevel level_ = {};

  std::array<float, 4> last_readings_ = {};
  bool boost_armed_ = true;
};

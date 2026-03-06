#pragma once

class Thermometer {
public:
  virtual ~Thermometer() = default;

  virtual void update() = 0;
};

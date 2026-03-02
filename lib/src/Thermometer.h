#pragma once

class Thermometer {
public:
  virtual ~Thermometer() = default;

  virtual bool connected() = 0;
  virtual void update() = 0;
};

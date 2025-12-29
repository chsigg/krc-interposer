#pragma once

class Thermometer {
public:
  virtual ~Thermometer() = default;

  virtual void start() = 0;
  virtual void stop() = 0;
  virtual bool connected() = 0;
};

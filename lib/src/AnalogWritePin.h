#pragma once

class AnalogWritePin {
public:
  virtual ~AnalogWritePin() = default;

  virtual void write(float value) = 0;
};

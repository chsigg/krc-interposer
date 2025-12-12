#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <ArduinoFake.h>
#include "StdOutLogger.h"

StdOutLogger logger;
Logger& Log = logger;

void delayUs(uint32_t us) { delayMicroseconds(us); }

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <ArduinoFake.h>

void delayUs(uint32_t us) { delayMicroseconds(us); }

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <ArduinoFake.h>
#include "StdOutLogger.h"

StdOutLogger logger;
Logger& Log = logger;

void delayUs(uint32_t us) { delayMicroseconds(us); }

REGISTER_EXCEPTION_TRANSLATOR(fakeit::FakeitException& ex) {
    return doctest::String(ex.what().c_str());
}

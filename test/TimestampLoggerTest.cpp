#include "TimestampLogger.h"
#include "BufferedLogger.h"
#include <doctest.h>
#include <string>
#include <ArduinoFake.h>

using namespace fakeit;

TEST_CASE("TimestampLogger Logic") {
  BufferedLogger buffer(128);
  TimestampLogger logger(buffer);

  // Use ArduinoFake to mock millis()
  When(Method(ArduinoFake(), millis)).AlwaysReturn(100);

  SUBCASE("Single line") {
    logger.log("Hello\n", 6);
    
    uint8_t out[32];
    size_t len = buffer.peek(out, 32);
    CHECK(std::string((char*)out, len) == "[   0.10] Hello\n");
  }

  SUBCASE("Split line") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(200);
    logger.log("Part 1 ", 7);
    logger.log("Part 2\n", 7);
    
    uint8_t out[64];
    size_t len = buffer.peek(out, 64);
    // Timestamp should only appear once at the beginning
    CHECK(std::string((char*)out, len) == "[   0.20] Part 1 Part 2\n");
    
    // Next line should get a new timestamp
    buffer.consume(len);
    When(Method(ArduinoFake(), millis)).AlwaysReturn(300);
    logger.log("Next\n", 5);
    len = buffer.peek(out, 64);
    CHECK(std::string((char*)out, len) == "[   0.30] Next\n");
  }

  SUBCASE("Numeric logs") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(400);
    logger.log(123L);
    logger.log("\n", 1);
    
    uint8_t out[32];
    size_t len = buffer.peek(out, 32);
    CHECK(std::string((char*)out, len) == "[   0.40] 123\n");
  }

  SUBCASE("Multi-line buffer") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(500);
    logger.log("Line 1\nLine 2\n", 14);
    
    uint8_t out[64];
    size_t len = buffer.peek(out, 64);
    CHECK(std::string((char*)out, len) == "[   0.50] Line 1\n[   0.50] Line 2\n");
  }
}

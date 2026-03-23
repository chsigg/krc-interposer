#include "BufferedLogger.h"
#include <doctest.h>
#include <string>

TEST_CASE("BufferedLogger Logic") {
  BufferedLogger logger(16); // 16 usable bytes

  SUBCASE("Initialization") {
    CHECK(logger.available() == 0);
  }

  SUBCASE("Single Byte Write/Read") {
    logger.log("A", 1);
    CHECK(logger.available() == 1);

    uint8_t out = 0;
    CHECK(logger.peek(&out, 1) == 1);
    CHECK(out == 'A');
    CHECK(logger.available() == 1); // Peek doesn't consume

    logger.consume(1);
    CHECK(logger.available() == 0);
  }

  SUBCASE("Stringification") {
    BufferedLogger long_logger(32);
    long_logger.log(123L);
    uint8_t out[10];
    size_t len = long_logger.peek(out, 10);
    CHECK(std::string((char*)out, len) == "123");

    BufferedLogger float_logger(32);
    float_logger.log(12.34f);
    len = float_logger.peek(out, 10);
    CHECK(std::string((char*)out, len) == "12.34");
  }

  SUBCASE("Wrap Around") {
    logger.log("1234567890", 10);
    logger.consume(10);
    CHECK(logger.available() == 0);

    // Write 10 bytes, will wrap around in the 16-byte buffer
    logger.log("ABCDEFGHIJ", 10);
    CHECK(logger.available() == 10);

    uint8_t out[10];
    CHECK(logger.peek(out, 10) == 10);
    CHECK(std::string((char*)out, 10) == "ABCDEFGHIJ");
  }
}

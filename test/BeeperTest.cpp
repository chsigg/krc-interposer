#include "Beeper.h"
#include "Buzzer.h"
#include <ArduinoFake.h>
#include <doctest.h>

using namespace fakeit;

namespace {
constexpr uint16_t kLowFreq = 800;
constexpr uint16_t kHighFreq = 1200;
constexpr uint16_t kToneDurationMs = 200;
} // namespace

TEST_CASE("Beeper Logic") {
  Mock<Buzzer> buzzer_mock;
  Beeper beeper(buzzer_mock.get());

  Fake(Method(buzzer_mock, enable));
  Fake(Method(buzzer_mock, disable));

  uint32_t current_time_ms = 1000;
  When(Method(ArduinoFake(), millis)).AlwaysDo([&]() {
    return current_time_ms;
  });
  auto advance_time = [&](uint32_t ms) {
    current_time_ms += ms;
    beeper.update();
  };

  SUBCASE("isIdle") {
    CHECK(beeper.isIdle());
    beeper.beep(Beeper::Signal::POWER_ON);
    CHECK_FALSE(beeper.isIdle());
    advance_time(kToneDurationMs); // First tone finishes
    CHECK_FALSE(beeper.isIdle());
    advance_time(kToneDurationMs); // Second tone finishes
    CHECK(beeper.isIdle());
  }

  SUBCASE("beep(POWER_ON) is LOW-HIGH") {
    beeper.beep(Beeper::Signal::POWER_ON);
    Verify(Method(buzzer_mock, enable).Using(kLowFreq)).Once();

    advance_time(kToneDurationMs);
    Verify(Method(buzzer_mock, enable).Using(kHighFreq)).Once();

    advance_time(kToneDurationMs);
    Verify(Method(buzzer_mock, disable)).Once();
    CHECK(beeper.isIdle());
  }

  SUBCASE("beep(POWER_OFF) is HIGH-LOW") {
    beeper.beep(Beeper::Signal::POWER_OFF);
    Verify(Method(buzzer_mock, enable).Using(kHighFreq)).Once();

    advance_time(kToneDurationMs);
    Verify(Method(buzzer_mock, enable).Using(kLowFreq)).Once();

    advance_time(kToneDurationMs);
    Verify(Method(buzzer_mock, disable)).Once();
    CHECK(beeper.isIdle());
  }

  SUBCASE("beep(CONNECTED) is HIGH-LOW-HIGH") {
    beeper.beep(Beeper::Signal::CONNECTED);
    Verify(Method(buzzer_mock, enable).Using(kHighFreq)).Once();

    advance_time(kToneDurationMs);
    Verify(Method(buzzer_mock, enable).Using(kLowFreq)).Once();

    advance_time(kToneDurationMs);
    Verify(Method(buzzer_mock, enable).Using(kHighFreq)).Twice();

    advance_time(kToneDurationMs);
    Verify(Method(buzzer_mock, disable)).Once();
    CHECK(beeper.isIdle());
  }

  SUBCASE("beep(DISCONNECTED) is a looping double tone") {
    beeper.beep(Beeper::Signal::DISCONNECTED);

    // First pair
    Verify(Method(buzzer_mock, enable).Using(kLowFreq)).Once();
    advance_time(kToneDurationMs); // PAUSE start
    Verify(Method(buzzer_mock, disable)).Once();
    advance_time(kToneDurationMs); // Tone 2 start
    Verify(Method(buzzer_mock, enable).Using(kLowFreq)).Twice();
    advance_time(kToneDurationMs); // SILENT start
    Verify(Method(buzzer_mock, disable)).Twice();

    advance_time(1000); // Back to start of loop
    Verify(Method(buzzer_mock, enable).Using(kLowFreq)).Exactly(3);
    CHECK_FALSE(beeper.isIdle());
  }
}

#include "Beeper.h"
#include "Buzzer.h"
#include <ArduinoFake.h>
#include <doctest.h>

using namespace fakeit;

namespace {
constexpr uint16_t LOW_FREQ = 800;
constexpr uint16_t HIGH_FREQ = 1200;
constexpr uint8_t TONE_DURATION_MS = 100;
} // namespace

TEST_CASE("Beeper Logic") {
  Mock<Buzzer> buzzer_mock;
  Beeper beeper(buzzer_mock.get());

  Fake(Method(buzzer_mock, enable));
  Fake(Method(buzzer_mock, disable));

  SUBCASE("beep() is instantaneous") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
    beeper.beep(Beeper::Signal::ACCEPT);
    Verify(Method(buzzer_mock, enable).Using(LOW_FREQ)).Once();
    beeper.update();
    VerifyNoOtherInvocations(buzzer_mock);
  }

  SUBCASE("beep(NONE) turns buzzer off") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
    beeper.beep(Beeper::Signal::ERROR);
    Verify(Method(buzzer_mock, enable).Using(LOW_FREQ)).Once();

    beeper.beep(Beeper::Signal::NONE);
    Verify(Method(buzzer_mock, disable)).Once();
  }

  SUBCASE("ACCEPT signal") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
    beeper.beep(Beeper::Signal::ACCEPT);
    Verify(Method(buzzer_mock, enable).Using(LOW_FREQ)).Once();

    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000 + TONE_DURATION_MS);
    beeper.update();
    Verify(Method(buzzer_mock, enable).Using(HIGH_FREQ)).Once();

    When(Method(ArduinoFake(), millis))
        .AlwaysReturn(1000 + 2 * TONE_DURATION_MS);
    beeper.update();
    Verify(Method(buzzer_mock, disable)).Once();
  }

  SUBCASE("REJECT signal") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
    beeper.beep(Beeper::Signal::REJECT);
    Verify(Method(buzzer_mock, enable).Using(HIGH_FREQ)).Once();

    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000 + TONE_DURATION_MS);
    beeper.update();
    Verify(Method(buzzer_mock, enable).Using(LOW_FREQ)).Once();

    When(Method(ArduinoFake(), millis))
        .AlwaysReturn(1000 + 2 * TONE_DURATION_MS);
    beeper.update();
    Verify(Method(buzzer_mock, disable)).Once();
  }

  SUBCASE("ERROR signal") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
    beeper.beep(Beeper::Signal::ERROR);
    Verify(Method(buzzer_mock, enable).Using(LOW_FREQ)).Once();

    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000 + TONE_DURATION_MS);
    beeper.update();
    Verify(Method(buzzer_mock, disable)).Once();

    When(Method(ArduinoFake(), millis))
        .AlwaysReturn(1000 + 2 * TONE_DURATION_MS);
    beeper.update();
    Verify(Method(buzzer_mock, enable).Using(LOW_FREQ)).Twice();

    When(Method(ArduinoFake(), millis))
        .AlwaysReturn(1000 + 3 * TONE_DURATION_MS);
    beeper.update();
    Verify(Method(buzzer_mock, disable)).Twice();
  }
}

#include "Beeper.h"
#include "Buzzer.h"
#include <ArduinoFake.h>
#include <doctest.h>

using namespace fakeit;

namespace {
constexpr uint16_t LOW_FREQ = 2000;
constexpr uint16_t HIGH_FREQ = 4000;
constexpr uint8_t TONE_DURATION_MS = 100;
} // namespace

TEST_CASE("Beeper Logic") {
  Mock<Buzzer> buzzer_mock;
  Beeper beeper(buzzer_mock.get());

  Fake(Method(buzzer_mock, enable));
  Fake(Method(buzzer_mock, disable));

  SUBCASE("ACCEPT signal") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
    beeper.beep(Beeper::Signal::ACCEPT);
    beeper.update();
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
    beeper.update();
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
    beeper.update();
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

#include "Blinker.h"
#include "DigitalWritePin.h"
#include <ArduinoFake.h>
#include <doctest.h>

using namespace fakeit;

TEST_CASE("Blinker Logic") {
  Mock<DigitalWritePin> led_mock;
  Blinker blinker(led_mock.get());

  Fake(Method(led_mock, set));

  SUBCASE("blink() is instantaneous") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
    blinker.blink(Blinker::Signal::ONCE);
    Verify(Method(led_mock, set).Using(PinState::Low)).Once();
    blinker.update();
    VerifyNoOtherInvocations(led_mock);
  }

  SUBCASE("blink(NONE) turns led off") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
    blinker.blink(Blinker::Signal::REPEAT);
    Verify(Method(led_mock, set).Using(PinState::Low)).Once();

    blinker.blink(Blinker::Signal::NONE);
    Verify(Method(led_mock, set).Using(PinState::High)).Once();
  }

  SUBCASE("ONCE signal") {
    // First, LED should be on
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
    blinker.blink(Blinker::Signal::ONCE);
    Verify(Method(led_mock, set).Using(PinState::Low)).Once();

    // After 100ms, LED should be off and signal finished
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000 + 100);
    blinker.update();
    Verify(Method(led_mock, set).Using(PinState::High)).Once();

    // After that, it should stay off
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000 + 200);
    blinker.update();
    VerifyNoOtherInvocations(led_mock);
  }

  SUBCASE("REPEAT signal") {
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);
    blinker.blink(Blinker::Signal::REPEAT);

    // First update, LED should be on for 100ms
    Verify(Method(led_mock, set).Using(PinState::Low)).Once();

    // After 100ms, LED should be off for 200ms
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000 + 100);
    blinker.update();
    Verify(Method(led_mock, set).Using(PinState::High)).Once();

    // In the middle of the pause
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000 + 100 + 500);
    blinker.update();
    VerifyNoOtherInvocations(led_mock);

    // After 1000ms pause, it should be on again
    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000 + 100 + 1000);
    blinker.update();
    Verify(Method(led_mock, set).Using(PinState::Low)).Twice();
  }
}

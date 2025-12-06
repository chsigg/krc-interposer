#include <doctest.h>
#include <ArduinoFake.h>
#include "DigiPot.h"
#include "DigitalWritePin.h"

using namespace fakeit;

TEST_CASE("DigiPot Driver") {
    Mock<DigitalWritePin> inc_mock;
    Mock<DigitalWritePin> ud_mock;
    Mock<DigitalWritePin> cs_mock;

    // Default behavior: do nothing
    Fake(Method(inc_mock, set));
    Fake(Method(ud_mock, set));
    Fake(Method(cs_mock, set));
    Fake(Method(ArduinoFake(), delayMicroseconds));

    DigiPot driver(inc_mock.get(), ud_mock.get(), cs_mock.get());

    SUBCASE("Initialization resets to 0") {
        // Verify initial state setup
        Verify(Method(cs_mock, set).Using(PinState::High)).AtLeast(1);

        // Verify reset sequence (Down, Select, Pulse > 99 times)
        Verify(Method(ud_mock, set).Using(PinState::Low)); // Down
        Verify(Method(cs_mock, set).Using(PinState::Low)); // Select

        // Should pulse at least NUM_STEPS times
        Verify(Method(inc_mock, set).Using(PinState::Low)).AtLeast(DigiPot::NUM_STEPS);
        Verify(Method(inc_mock, set).Using(PinState::High)).AtLeast(DigiPot::NUM_STEPS);

        Verify(Method(cs_mock, set).Using(PinState::High)); // Deselect

        CHECK(driver.getPosition() == 0.0f);
    }

    SUBCASE("Set step increments correctly") {
        // Force internal state to 0 (constructor default)
        inc_mock.ClearInvocationHistory();
        ud_mock.ClearInvocationHistory();
        cs_mock.ClearInvocationHistory();

        float position = 10 / (DigiPot::NUM_STEPS-1.0f);

        driver.setPosition(position);

        Verify(Method(ud_mock, set).Using(PinState::High));  // Up
        Verify(Method(cs_mock, set).Using(PinState::Low)); // Select
        Verify(Method(inc_mock, set).Using(PinState::Low)).Exactly(10); // 10 pulses
        Verify(Method(cs_mock, set).Using(PinState::High));  // Deselect

        CHECK(driver.getPosition() == position);
    }

    SUBCASE("Set step clamps to limits") {
        driver.setPosition(1.5f);
        CHECK(driver.getPosition() == 1.0f);

        driver.setPosition(-0.5f);
        CHECK(driver.getPosition() == 0.0f);
    }
}

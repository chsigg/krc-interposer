#include <doctest.h>
#include <fakeit.hpp>
#include "Beeper.h"
#include "Buzzer.h"

using namespace fakeit;

// Mock millis for testing
static uint32_t current_millis = 0;
extern "C" uint32_t millis() { return current_millis; }

TEST_CASE("Beeper Logic") {
    Mock<Buzzer> buzzer_mock;
    Beeper beeper(buzzer_mock.get());

    current_millis = 1000;

    SUBCASE("Beep enables buzzer for given duration") {
        Fake(Method(buzzer_mock, enable));
        Fake(Method(buzzer_mock, disable));

        beeper.beep(100); // end_time = 1100

        Verify(Method(buzzer_mock, enable));

        current_millis = 1050;
        beeper.update();
        Verify(Method(buzzer_mock, disable)).Exactly(0);

        current_millis = 1100;
        beeper.update();
        Verify(Method(buzzer_mock, disable)).Exactly(1);

        // Should not disable again
        beeper.update();
        Verify(Method(buzzer_mock, disable)).Exactly(1);
    }
}

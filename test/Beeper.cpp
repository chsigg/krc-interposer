#include <doctest.h>
#include <ArduinoFake.h>
#include "Beeper.h"
#include "Buzzer.h"

using namespace fakeit;

TEST_CASE("Beeper Logic") {
    Mock<Buzzer> buzzer_mock;
    Beeper beeper(buzzer_mock.get());

    When(Method(ArduinoFake(), millis)).AlwaysReturn(1000);

    SUBCASE("Beep enables buzzer for given duration") {
        Fake(Method(buzzer_mock, enable));
        Fake(Method(buzzer_mock, disable));

        beeper.beep(100); // end_time = 1100

        Verify(Method(buzzer_mock, enable));

        When(Method(ArduinoFake(), millis)).AlwaysReturn(1050);
        beeper.update();
        Verify(Method(buzzer_mock, disable)).Exactly(0);

        When(Method(ArduinoFake(), millis)).AlwaysReturn(1100);
        beeper.update();
        Verify(Method(buzzer_mock, disable)).Exactly(1);

        // Should not disable again
        beeper.update();
        Verify(Method(buzzer_mock, disable)).Exactly(1);
    }
}

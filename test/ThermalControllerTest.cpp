#include <ArduinoFake.h>
#include <doctest.h>
#include "ThermalController.h"
#include "TrendAnalyzer.h"
#include <fakeit.hpp>

using namespace fakeit;

TEST_CASE("ThermalController") {
    Mock<TrendAnalyzer> analyzer_mock;
    ThermalConfig config;
    config.p_factor = 0.1f;
    config.heat_loss_factor = 0.01f;
    config.system_lag_ms = 5000;
    config.ambient_temp = 20.0f;

    ThermalController controller(analyzer_mock.get(), config);

    uint32_t current_time = 10000;
    When(Method(ArduinoFake(), millis)).AlwaysDo([&]() { return current_time; });

    SUBCASE("Initial state") {
        When(Method(analyzer_mock, getValue)).AlwaysReturn(0.0f);
        CHECK(controller.getTargetTemp() == doctest::Approx(20.0f));
        CHECK(controller.getPower() == doctest::Approx(0.0f));
    }

    SUBCASE("Target temperature updates") {
        controller.setTargetTemp(50.0f);
        CHECK(controller.getTargetTemp() == doctest::Approx(50.0f));
    }

    SUBCASE("Power calculation with P-factor and loss") {
        controller.setTargetTemp(50.0f);

        When(Method(analyzer_mock, getSlope)).AlwaysReturn(0.0f);
        When(Method(analyzer_mock, getValue)).AlwaysReturn(30.0f);

        controller.update();
        CHECK(controller.getPower() == doctest::Approx(1.0f));

        controller.setTargetTemp(35.0f);
        controller.update();
        CHECK(controller.getPower() == doctest::Approx(0.6f));
    }

    SUBCASE("System lag compensation (prediction)") {
        controller.setTargetTemp(50.0f);

        When(Method(analyzer_mock, getSlope)).AlwaysReturn(0.001f);
        When(Method(analyzer_mock, getValue).Using(current_time + config.system_lag_ms)).AlwaysReturn(45.0f);
        When(Method(analyzer_mock, getValue).Using(current_time)).AlwaysReturn(40.0f);

        controller.update();
        CHECK(controller.getPower() == doctest::Approx(0.7f));
    }
}

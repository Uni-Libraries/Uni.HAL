//
// Includes
//

// stdlib
#include <cstdint>

// catch2
#include <catch2/catch_test_macros.hpp>

// uni.hal
#include <uni_hal.h>


//
// tests
//

TEST_CASE("can_timing_gives_exact_bitrate", "[hal_can]") {
    const uint32_t clocks_hz[] = {8'000'000U, 16'000'000U, 32'000'000U, 36'000'000U, 40'000'000U, 48'000'000U, 80'000'000U};
    const uint32_t bitrates[] = {125'000U, 250'000U, 500'000U, 1'000'000U};

    for (uint32_t clock_hz : clocks_hz) {
        for (uint32_t bitrate : bitrates) {
            uni_hal_can_timing_t timing = {};
            INFO("clock " << clock_hz << " Hz, bit rate " << bitrate);
            REQUIRE(uni_hal_can_timing_calc(clock_hz, bitrate, &timing));

            const uint32_t quanta = 1U + timing.bs1 + timing.bs2;
            REQUIRE(timing.prescaler >= 1U);
            REQUIRE(timing.prescaler <= 1024U);
            REQUIRE(timing.bs1 >= 1U);
            REQUIRE(timing.bs1 <= 16U);
            REQUIRE(timing.bs2 >= 1U);
            REQUIRE(timing.bs2 <= 8U);
            REQUIRE(timing.sjw >= 1U);
            REQUIRE(timing.sjw <= timing.bs2);
            REQUIRE(clock_hz == bitrate * timing.prescaler * quanta);

            // sample point between 75 % and 90 %
            const uint32_t sample_point = ((1U + timing.bs1) * 1000U) / quanta;
            REQUIRE(sample_point >= 750U);
            REQUIRE(sample_point <= 900U);
        }
    }
}

TEST_CASE("can_timing_fw_ktb_bus", "[hal_can]") {
    // 32 MHz APB1, 500 kbit/s: 16 quanta with the sample point at exactly 87.5 %
    uni_hal_can_timing_t timing = {};
    REQUIRE(uni_hal_can_timing_calc(32'000'000U, 500'000U, &timing));
    REQUIRE(timing.prescaler == 4U);
    REQUIRE(timing.bs1 == 13U);
    REQUIRE(timing.bs2 == 2U);
}

TEST_CASE("can_timing_rejects_inexact_bitrate", "[hal_can]") {
    uni_hal_can_timing_t timing = {};
    REQUIRE_FALSE(uni_hal_can_timing_calc(32'000'000U, 0U, &timing));
    REQUIRE_FALSE(uni_hal_can_timing_calc(0U, 500'000U, &timing));
    REQUIRE_FALSE(uni_hal_can_timing_calc(32'000'000U, 500'000U, nullptr));
    // 33 MHz / 500 kbit/s = 66 ticks per bit: no divisor between 8 and 25 quanta except 11 and 22
    REQUIRE(uni_hal_can_timing_calc(33'000'000U, 500'000U, &timing));
    // 7 MHz / 1 Mbit/s = 7 ticks per bit: fewer than the 8 quanta a bit needs here
    REQUIRE_FALSE(uni_hal_can_timing_calc(7'000'000U, 1'000'000U, &timing));
}

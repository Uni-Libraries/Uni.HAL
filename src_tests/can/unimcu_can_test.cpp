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

TEST_CASE("can_timing_data_phase", "[hal_can]") {
    const uint32_t clocks_hz[] = {20'000'000U, 40'000'000U, 80'000'000U};
    const uint32_t bitrates[] = {1'000'000U, 2'000'000U, 4'000'000U, 5'000'000U, 8'000'000U};

    for (uint32_t clock_hz : clocks_hz) {
        for (uint32_t bitrate : bitrates) {
            uni_hal_can_timing_t timing = {};
            const bool found = uni_hal_can_timing_calc_data(clock_hz, bitrate, &timing);

            INFO("clock " << clock_hz << " Hz, data bit rate " << bitrate);
            if (clock_hz / bitrate < 5U) {
                // fewer than five time quanta per bit: not possible
                REQUIRE_FALSE(found);
                continue;
            }
            REQUIRE(found);

            const uint32_t quanta = 1U + timing.bs1 + timing.bs2;
            REQUIRE(timing.prescaler >= 1U);
            REQUIRE(timing.prescaler <= 32U);
            REQUIRE(timing.bs1 >= 1U);
            REQUIRE(timing.bs1 <= 32U);
            REQUIRE(timing.bs2 >= 1U);
            REQUIRE(timing.bs2 <= 16U);
            REQUIRE(timing.sjw >= 1U);
            REQUIRE(timing.sjw <= timing.bs2);
            REQUIRE(clock_hz == bitrate * timing.prescaler * quanta);

            // sample point between 70 % and 85 %
            const uint32_t sample_point = ((1U + timing.bs1) * 1000U) / quanta;
            REQUIRE(sample_point >= 700U);
            REQUIRE(sample_point <= 850U);
        }
    }
}

TEST_CASE("can_timing_nominal_keeps_narrow_jump_width", "[hal_can]") {
    // the arbitration phase timing is the one bxCAN users have: unchanged by the data phase work
    uni_hal_can_timing_t timing = {};
    REQUIRE(uni_hal_can_timing_calc(32'000'000U, 500'000U, &timing));
    REQUIRE(timing.sjw == 1U);
}

TEST_CASE("can_dlc_classic", "[hal_can]") {
    for (uint32_t length = 0U; length <= 8U; length++) {
        REQUIRE(uni_hal_can_dlc_from_length(length, false) == length);
        REQUIRE(uni_hal_can_dlc_to_length(length, false) == length);
    }
    // a classic frame carries at most 8 bytes, whatever its length code says
    REQUIRE(uni_hal_can_dlc_from_length(12U, false) == UINT8_MAX);
    for (uint32_t dlc = 9U; dlc <= 15U; dlc++) {
        REQUIRE(uni_hal_can_dlc_to_length(dlc, false) == 8U);
    }
}

TEST_CASE("can_dlc_fd", "[hal_can]") {
    const uint32_t lengths[] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U};

    // every data length code has exactly one length, and back
    for (uint32_t dlc = 0U; dlc <= 15U; dlc++) {
        REQUIRE(uni_hal_can_dlc_to_length(dlc, true) == lengths[dlc]);
        REQUIRE(uni_hal_can_dlc_from_length(lengths[dlc], true) == dlc);
    }

    // lengths between the steps do not exist on the bus
    for (uint32_t length : {9U, 10U, 11U, 13U, 17U, 31U, 33U, 63U, 65U, 100U}) {
        REQUIRE(uni_hal_can_dlc_from_length(length, true) == UINT8_MAX);
    }
}


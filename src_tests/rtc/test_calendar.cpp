#include <catch2/catch_test_macros.hpp>

#include "rtc/uni_hal_rtc.h"

namespace {
void require_calendar(const uni_hal_rtc_context_t &ctx, const uni_hal_rtc_datetime_t &expected) {
    uni_hal_rtc_datetime_t actual{};
    REQUIRE(uni_hal_rtc_get(&ctx, &actual));
    REQUIRE(actual.year == expected.year);
    REQUIRE(actual.month == expected.month);
    REQUIRE(actual.day == expected.day);
    REQUIRE(actual.weekday == expected.weekday);
    REQUIRE(actual.hours == expected.hours);
    REQUIRE(actual.minutes == expected.minutes);
    REQUIRE(actual.seconds == expected.seconds);
    REQUIRE(actual.milliseconds == 0U);
}
} // namespace

TEST_CASE("RTC accepts calendar field limits and ignores the read-only fraction", "[rtc]") {
    uni_hal_rtc_context_t ctx{};
    REQUIRE(uni_hal_rtc_init(&ctx));
    const uni_hal_rtc_datetime_t limits[] = {
        {2000, 1, 1, 6, 0, 0, 0, 999},
        {2099, 12, 31, 4, 23, 59, 59, 1000},
        {2024, 12, 29, 7, 0, 0, 0, 0},
        {2024, 12, 30, 1, 0, 0, 0, 0},
    };
    for (const auto &datetime : limits) {
        REQUIRE(uni_hal_rtc_set(&ctx, &datetime));
        require_calendar(ctx, datetime);
    }
}

TEST_CASE("RTC rejects out-of-range fields and preserves the previous calendar", "[rtc]") {
    uni_hal_rtc_context_t ctx{};
    REQUIRE(uni_hal_rtc_init(&ctx));
    const uni_hal_rtc_datetime_t previous{2024, 2, 29, 4, 12, 34, 56, 0};
    REQUIRE(uni_hal_rtc_set(&ctx, &previous));

    const auto rejected = [&]<typename T>(T uni_hal_rtc_datetime_t::*field, unsigned value) {
        auto invalid = previous;
        invalid.*field = static_cast<T>(value);
        CHECK_FALSE(uni_hal_rtc_set(&ctx, &invalid));
        require_calendar(ctx, previous);
    };
    rejected(&uni_hal_rtc_datetime_t::year, 1999U);
    rejected(&uni_hal_rtc_datetime_t::year, 2100U);
    rejected(&uni_hal_rtc_datetime_t::month, 0U);
    rejected(&uni_hal_rtc_datetime_t::month, 13U);
    rejected(&uni_hal_rtc_datetime_t::day, 0U);
    rejected(&uni_hal_rtc_datetime_t::day, 32U);
    rejected(&uni_hal_rtc_datetime_t::weekday, 0U);
    rejected(&uni_hal_rtc_datetime_t::weekday, 8U);
    rejected(&uni_hal_rtc_datetime_t::hours, 24U);
    rejected(&uni_hal_rtc_datetime_t::minutes, 60U);
    rejected(&uni_hal_rtc_datetime_t::seconds, 60U);
    REQUIRE_FALSE(uni_hal_rtc_set(&ctx, nullptr));
    require_calendar(ctx, previous);
}

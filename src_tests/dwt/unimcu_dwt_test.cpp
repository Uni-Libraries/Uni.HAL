//
// Includes
//

// catch2
#include <catch2/catch_test_macros.hpp>

// uni.hal
#include <uni_hal.h>


//
// tests
//

TEST_CASE("dwt_compare_wraparound", "[hal_dwt]") {
    REQUIRE(uni_hal_dwt_compare(0U, 0U) == 0U);
    REQUIRE(uni_hal_dwt_compare(10U, 12U) == 2U);
    REQUIRE(uni_hal_dwt_compare(0xFFFFFFFEU, 1U) == 3U);
}


TEST_CASE("dwt_timeout_ms_to_tick_minimum_one", "[hal_dwt]") {
    REQUIRE(uni_hal_dwt_timeout_ms_to_tick(0U) == 1U);
    REQUIRE(uni_hal_dwt_timeout_ms_to_tick(1U) == 1U);
    REQUIRE(uni_hal_dwt_timeout_ms_to_tick(25U) == 25U);
}


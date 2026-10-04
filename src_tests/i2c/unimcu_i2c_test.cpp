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
// helpers
//

namespace {
    struct Decoded {
        uint32_t presc;
        uint32_t scldel;
        uint32_t sdadel;
        uint32_t sclh;
        uint32_t scll;
    };

    Decoded decode(uint32_t timing) {
        return {timing >> 28U, (timing >> 20U) & 0xFU, (timing >> 16U) & 0xFU, (timing >> 8U) & 0xFFU, timing & 0xFFU};
    }

    struct Spec {
        uni_hal_i2c_speed_e speed;
        double freq_hz;
        double t_low_min_ns;
        double t_high_min_ns;
        double t_su_dat_min_ns;
        double t_rise_max_ns;
    };

    const Spec specs[] = {
        {UNI_HAL_I2C_SPEED_100KHZ, 100e3, 4700.0, 4000.0, 250.0, 1000.0},
        {UNI_HAL_I2C_SPEED_400KHZ, 400e3, 1300.0, 600.0, 100.0, 300.0},
        {UNI_HAL_I2C_SPEED_1MHZ, 1e6, 500.0, 260.0, 50.0, 120.0},
    };

    const uint32_t clocks_hz[] = {8'000'000U,  16'000'000U, 25'000'000U, 32'000'000U, 48'000'000U,
                                  50'000'000U, 54'000'000U, 64'000'000U, 80'000'000U, 100'000'000U, 120'000'000U};
}


//
// tests
//

TEST_CASE("i2c_timing_meets_bus_limits", "[hal_i2c]") {
    for (const Spec& spec : specs) {
        for (uint32_t clock_hz : clocks_hz) {
            const uint32_t timing = uni_hal_i2c_timing_calc(clock_hz, spec.speed);
            if (timing == 0U) {
                // only acceptable when the clock really is too slow: tI2CCLK >= (tLOW - 260 ns) / 4
                const double period_ns = 1e9 / clock_hz;
                REQUIRE(period_ns * 4.0 >= spec.t_low_min_ns - 260.0);
                continue;
            }

            const Decoded d = decode(timing);
            const double presc_ns = (d.presc + 1U) * 1e9 / clock_hz;
            const double t_low_ns = (d.scll + 1U) * presc_ns;
            const double t_high_ns = (d.sclh + 1U) * presc_ns;
            const double t_scldel_ns = (d.scldel + 1U) * presc_ns;

            INFO("clock " << clock_hz << " Hz, target " << spec.freq_hz << " Hz, timing 0x" << std::hex << timing);
            REQUIRE(t_low_ns >= spec.t_low_min_ns);
            REQUIRE(t_high_ns >= spec.t_high_min_ns);
            REQUIRE(t_scldel_ns >= spec.t_rise_max_ns + spec.t_su_dat_min_ns);
            REQUIRE(d.sdadel == 0U);

            // never faster than asked for, even with the shortest synchronisation delays
            const double sync_min_ns = 2.0 * (50.0 + 2.0 * 1e9 / clock_hz);
            const double freq_max_hz = 1e9 / (t_low_ns + t_high_ns + sync_min_ns);
            REQUIRE(freq_max_hz <= spec.freq_hz * 1.001);

            // and not needlessly slow
            REQUIRE(freq_max_hz >= spec.freq_hz * 0.75);
        }
    }
}

TEST_CASE("i2c_timing_rejects_unusable_clock", "[hal_i2c]") {
    REQUIRE(uni_hal_i2c_timing_calc(0U, UNI_HAL_I2C_SPEED_100KHZ) == 0U);
    // 1 MHz needs a kernel clock above 16.7 MHz
    REQUIRE(uni_hal_i2c_timing_calc(8'000'000U, UNI_HAL_I2C_SPEED_1MHZ) == 0U);
    REQUIRE(uni_hal_i2c_timing_calc(25'000'000U, UNI_HAL_I2C_SPEED_1MHZ) != 0U);
}

#include <catch2/catch_test_macros.hpp>

#include "core_cm4.h"
#include "rcc/uni_hal_rcc.h"
#include "rcc/uni_hal_rcc_stm32l4.h"

extern "C" {
extern bool rcc_test_pll_locks, rcc_test_can_switch, rcc_test_fault_on_switch;
extern bool rcc_test_flash_accepts, rcc_test_flash_can_lower;
extern uint32_t rcc_test_tick_reads, rcc_test_tick_inits, SystemCoreClock;
void rcc_test_reset(void);
void rcc_test_fail_hse(void);
void rcc_test_hsi_not_ready(void);
void rcc_test_voltage_range2(void);
uint32_t rcc_test_m(void);
uint32_t rcc_test_n(void);
uint32_t rcc_test_p(void);
uint32_t rcc_test_q(void);
void rcc_test_use_fixed_p(bool div17);
bool rcc_test_css_acknowledged(void);
bool rcc_test_requested_pll(void);
}

static void configure(uint32_t m = 3, uint32_t n = 40, uint32_t p = 0, uint32_t q = 0, uint32_t r = 2) {
    uni_hal_rcc_stm32l4_config_t config{};
    config.hse_enable = true;
    config.hse_css = true;
    config.pll[0].m = m;
    config.pll[0].n = n;
    config.pll[0].p = p;
    config.pll[0].q = q;
    config.pll[0].r = r;
    config.timeout.hsi = 2;
    config.timeout.pll = 2;
    REQUIRE(uni_hal_rcc_stm32l4_config_set(&config));
    // The NMI can arrive after the config setter's caller has returned.
}

TEST_CASE("L496 CSS preserves 80 MHz with 12 MHz HSE and 16 MHz HSI") {
    rcc_test_reset();
    configure();
    REQUIRE(uni_hal_rcc_init());
    REQUIRE(SystemCoreClock == 80'000'000);
    REQUIRE(rcc_test_m() == 3);

    // Model an RTOS-owned SysTick on HCLK/8. Recovery must retain its settings.
    SysTick->CTRL = 3U;
    SysTick->LOAD = 9'999;
    SysTick->VAL = 123;
    auto tick_reads = rcc_test_tick_reads;
    rcc_test_fail_hse();
    auto status = uni_hal_rcc_stm32l4_status_get();
    REQUIRE(rcc_test_css_acknowledged());
    REQUIRE(status.inited);
    REQUIRE_FALSE(status.hse_inited);
    REQUIRE(status.hsi_inited);
    REQUIRE(status.pll_inited);
    REQUIRE(status.sys_inited);
    REQUIRE(rcc_test_m() == 4);
    REQUIRE(rcc_test_n() == 40);
    REQUIRE(SystemCoreClock == 80'000'000);
    REQUIRE(SysTick->CTRL == 3U);
    REQUIRE(SysTick->LOAD == 9'999);
    REQUIRE(SysTick->VAL == 123);
    REQUIRE(rcc_test_tick_inits == 1);
    REQUIRE(rcc_test_tick_reads == tick_reads);
}

TEST_CASE("L496 CSS uses an exact PLL ratio or leaves SYSCLK on HSI") {
    rcc_test_reset();
    SECTION("exact VCO requires a different multiplier") {
        configure(2, 24);
        REQUIRE(uni_hal_rcc_init());
        rcc_test_fail_hse();
        REQUIRE(uni_hal_rcc_stm32l4_status_get().pll_inited);
        REQUIRE(SystemCoreClock == 72'000'000);
    }
    SECTION("102 MHz VCO cannot be generated exactly from HSI") {
        configure(2, 17);
        REQUIRE(uni_hal_rcc_init());
        REQUIRE(SystemCoreClock == 51'000'000);
        rcc_test_fail_hse();
        REQUIRE_FALSE(uni_hal_rcc_stm32l4_status_get().pll_inited);
        REQUIRE(uni_hal_rcc_is_inited());
        REQUIRE(SystemCoreClock == 16'000'000);
    }
    SECTION("loss of PLL lock with frozen SysTick") {
        configure();
        REQUIRE(uni_hal_rcc_init());
        rcc_test_pll_locks = false;
        auto tick_reads = rcc_test_tick_reads;
        rcc_test_fail_hse();
        REQUIRE(rcc_test_tick_reads == tick_reads);
        REQUIRE_FALSE(uni_hal_rcc_stm32l4_status_get().pll_inited);
        REQUIRE(uni_hal_rcc_is_inited());
        REQUIRE(SystemCoreClock == 16'000'000);
    }
    SECTION("HSI never becomes ready") {
        configure();
        REQUIRE(uni_hal_rcc_init());
        rcc_test_hsi_not_ready();
        auto tick_reads = rcc_test_tick_reads;
        rcc_test_fail_hse();
        REQUIRE(rcc_test_tick_reads == tick_reads);
        REQUIRE_FALSE(uni_hal_rcc_stm32l4_status_get().hsi_inited);
        REQUIRE_FALSE(uni_hal_rcc_is_inited());
        REQUIRE(SystemCoreClock == 4'000'000);
        REQUIRE(SysTick->LOAD == 3'999);
    }
    SECTION("unacknowledged clock switch with frozen SysTick") {
        configure();
        REQUIRE(uni_hal_rcc_init());
        rcc_test_can_switch = false;
        auto tick_reads = rcc_test_tick_reads;
        rcc_test_fail_hse();
        REQUIRE(rcc_test_tick_reads == tick_reads);
        REQUIRE_FALSE(uni_hal_rcc_stm32l4_status_get().sys_inited);
        REQUIRE_FALSE(uni_hal_rcc_is_inited());
        REQUIRE_FALSE(rcc_test_requested_pll());
        REQUIRE(SystemCoreClock == 4'000'000);
        REQUIRE(SysTick->LOAD == 3'999);
    }
}

TEST_CASE("L496 CSS preserves the running scheduler tick across an HCLK change") {
    rcc_test_reset();
    configure();
    REQUIRE(uni_hal_rcc_init());
    REQUIRE(SystemCoreClock == 80'000'000);
    rcc_test_pll_locks = false;
    SysTick->CTRL = 3U;  // timer and IRQ enabled, clocked at HCLK/8

    SECTION("250 Hz survives the switch to direct HSI") {
        SysTick->LOAD = 39'999;  // 80 MHz / 8 / 250 Hz - 1
        rcc_test_fail_hse();
        REQUIRE(SystemCoreClock == 16'000'000);
        REQUIRE(SysTick->LOAD == 7'999);
        REQUIRE(SysTick->CTRL == 3U);
        REQUIRE(rcc_test_tick_inits == 1);
    }
    SECTION("a stopped SysTick stays stopped after recovery") {
        SysTick->CTRL = 0U;
        SysTick->LOAD = 0U;
        rcc_test_fail_hse();
        REQUIRE(SystemCoreClock == 16'000'000);
        REQUIRE(SysTick->CTRL == 0U);
        REQUIRE(SysTick->LOAD == 0U);
    }
    SECTION("an unrepresentably short period keeps the minimum running reload") {
        SysTick->LOAD = 1;
        rcc_test_fail_hse();
        REQUIRE(SystemCoreClock == 16'000'000);
        REQUIRE(SysTick->LOAD == 1);
    }
}

TEST_CASE("L496 clock initialization reports failures and handles an interrupted switch") {
    rcc_test_reset();
    configure();
    SECTION("an out-of-range PLL output falls back to the oscillator") {
        configure(3, 41);  // 82 MHz exceeds the L496 limit
        REQUIRE(uni_hal_rcc_init());
        REQUIRE_FALSE(uni_hal_rcc_stm32l4_status_get().pll_inited);
        REQUIRE(SystemCoreClock == 12'000'000);
    }
    SECTION("range 2 cannot run the 80 MHz PLL configuration") {
        rcc_test_voltage_range2();
        REQUIRE(uni_hal_rcc_init());
        REQUIRE_FALSE(uni_hal_rcc_stm32l4_status_get().pll_inited);
        REQUIRE(SystemCoreClock == 12'000'000);
    }
    SECTION("flash latency cannot be raised before SysTick startup") {
        rcc_test_flash_accepts = false;
        REQUIRE_FALSE(uni_hal_rcc_init());
        REQUIRE_FALSE(uni_hal_rcc_is_inited());
        REQUIRE_FALSE(rcc_test_requested_pll());
        REQUIRE(rcc_test_tick_inits == 0);
    }
    SECTION("final flash latency cannot be applied") {
        configure(2, 16);
        rcc_test_flash_can_lower = false;
        REQUIRE_FALSE(uni_hal_rcc_init());
        REQUIRE_FALSE(uni_hal_rcc_is_inited());
        REQUIRE(SystemCoreClock == 48'000'000);
    }
    SECTION("system clock never acknowledges the switch") {
        rcc_test_can_switch = false;
        REQUIRE_FALSE(uni_hal_rcc_init());
        REQUIRE_FALSE(uni_hal_rcc_is_inited());
        REQUIRE_FALSE(uni_hal_rcc_stm32l4_status_get().sys_inited);
        REQUIRE_FALSE(rcc_test_requested_pll());
        REQUIRE(SystemCoreClock == 4'000'000);
    }
    SECTION("reinitialization leaves the active PLL configuration intact") {
        REQUIRE(uni_hal_rcc_init());
        configure(2, 24);
        REQUIRE(uni_hal_rcc_init());
        REQUIRE_FALSE(uni_hal_rcc_stm32l4_status_get().pll_inited);
        REQUIRE(rcc_test_m() == 3);
        REQUIRE(rcc_test_n() == 40);
        REQUIRE(rcc_test_tick_inits == 1);
        REQUIRE(SystemCoreClock == 12'000'000);
    }
    SECTION("CSS interrupt while switching during initialization") {
        rcc_test_fault_on_switch = true;
        REQUIRE(uni_hal_rcc_init());
        auto status = uni_hal_rcc_stm32l4_status_get();
        REQUIRE_FALSE(status.hse_inited);
        REQUIRE(status.pll_inited);
        REQUIRE(status.sys_inited);
        REQUIRE(rcc_test_m() == 4);
        REQUIRE(SystemCoreClock == 80'000'000);
    }
}

TEST_CASE("L496 programs and validates the requested main PLL outputs") {
    rcc_test_reset();
    SECTION("P and Q keep their dividers after CSS switches to HSI") {
        configure(2, 24, 3, 6);
        REQUIRE(uni_hal_rcc_init());
        REQUIRE(rcc_test_p() == 3);
        REQUIRE(rcc_test_q() == 6);
        rcc_test_fail_hse();
        REQUIRE(uni_hal_rcc_stm32l4_status_get().pll_inited);
        REQUIRE(SystemCoreClock == 72'000'000);
        REQUIRE(rcc_test_p() == 3);
        REQUIRE(rcc_test_q() == 6);
    }
    SECTION("zero leaves optional outputs disabled on startup") {
        configure();
        REQUIRE(uni_hal_rcc_init());
        REQUIRE(rcc_test_p() == 0);
        REQUIRE(rcc_test_q() == 0);
    }
    SECTION("a running PLL is adopted only if its requested outputs match") {
        configure(2, 24, 17, 4);
        REQUIRE(uni_hal_rcc_init());
        rcc_test_use_fixed_p(true); // PLLPDIV=0 selects the fixed 7/17 encoding.
        REQUIRE(uni_hal_rcc_init());
        REQUIRE(uni_hal_rcc_stm32l4_status_get().pll_inited);
        configure(2, 24, 17, 8);
        REQUIRE(uni_hal_rcc_init());
        REQUIRE_FALSE(uni_hal_rcc_stm32l4_status_get().pll_inited);
        REQUIRE(rcc_test_p() == 17);
        REQUIRE(rcc_test_q() == 4);
    }
    SECTION("invalid P and Q requests leave SYSCLK on the oscillator") {
        struct { uint32_t p, q; } requests[] = {{1, 0}, {32, 0}, {0, 3}, {0, 10}, {2, 0}, {0, 2}};
        for (auto request : requests) {
            CAPTURE(request.p, request.q);
            rcc_test_reset();
            configure(2, 32, request.p, request.q, 4); // 192 MHz VCO, 48 MHz SYSCLK
            REQUIRE(uni_hal_rcc_init());
            REQUIRE_FALSE(uni_hal_rcc_stm32l4_status_get().pll_inited);
            REQUIRE(SystemCoreClock == 12'000'000);
        }
    }
}

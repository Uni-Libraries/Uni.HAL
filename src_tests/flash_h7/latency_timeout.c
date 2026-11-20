#include <stm32h7xx.h>
#include "../arm/semihost_test.h"

static PWR_TypeDef pwr;
static FLASH_TypeDef flash;
#undef PWR_BASE
#define PWR_BASE ((uintptr_t)&pwr)
#undef FLASH_R_BASE
#define FLASH_R_BASE ((uintptr_t)&flash)
#include <stm32h7xx_ll_system.h>

static uint32_t requested_latency, reads, ready_at;
static uint32_t milliseconds, ms_step, cycles, cycle_step;
static bool dwt_running;

static void request_latency(uint32_t latency) {
    requested_latency = latency;
    reads = 0U;
}

static uint32_t read_latency(void) {
    if (++reads == ready_at) {
        LL_FLASH_SetLatency(requested_latency);
    }
    return LL_FLASH_GetLatency();
}

#define LL_FLASH_SetLatency request_latency
#define LL_FLASH_GetLatency read_latency
#define uni_hal_systick_get_ms test_ms
#define uni_hal_dwt_is_inited test_dwt_inited
#define uni_hal_dwt_get_tick test_cycle
#include "flash/uni_hal_flash_stm32h7.c"

uint32_t test_ms(void) {
    milliseconds += ms_step;
    return milliseconds;
}

bool test_dwt_inited(void) {
    return dwt_running;
}

uint32_t test_cycle(void) {
    cycles += cycle_step;
    return cycles;
}

static unsigned check(void) {
    SystemCoreClock = 1000000U;
    pwr.D3CR = LL_PWR_REGU_VOLTAGE_SCALE1;
    // The hardware eventually accepts the write even if the time sources stay frozen.
    ready_at = 500000U;
    CHECK(!uni_hal_flash_configure_latency_auto(100000000U));
    CHECK(reads < ready_at && (flash.ACR & FLASH_ACR_LATENCY) == LL_FLASH_LATENCY_0);

    // A stalled initialized DWT must not hide the ordinary deadline across SysTick wrap.
    dwt_running = true;
    milliseconds = UINT32_MAX - 50U;
    ms_step = 25U;
    CHECK(!uni_hal_flash_configure_latency_auto(100000000U));
    CHECK(reads < 10U);

    // With SysTick masked, a running DWT still measures the real deadline across CYCCNT wrap.
    ms_step = 0U;
    cycles = UINT32_MAX - 50000U;
    cycle_step = 25000U;
    CHECK(!uni_hal_flash_configure_latency_auto(100000000U));
    CHECK(reads < 10U);

    cycle_step = 0U;
    ready_at = 32U;
    CHECK(uni_hal_flash_configure_latency_auto(100000000U));
    CHECK(reads == ready_at && requested_latency == LL_FLASH_LATENCY_1);
    return 0;
}

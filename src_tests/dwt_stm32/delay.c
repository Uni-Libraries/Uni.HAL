#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif
#include <stdbool.h>
#include "../arm/semihost_test.h"

static uint64_t cycles;
static uint32_t step;
static bool first_read;
static DWT_Type counter;

static DWT_Type *sample_counter(void) {
    if (!first_read) {
        cycles += step;
    }
    first_read = false;
    counter.CYCCNT = (uint32_t)cycles + UINT32_MAX - 1U;
    return &counter;
}

#undef DWT
#define DWT sample_counter()
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "dwt/uni_hal_dwt_stm32h7.c"
#else
#include "dwt/uni_hal_dwt_stm32l4.c"
#endif

static void start(uint32_t clock, uint32_t cycles_per_read) {
    SystemCoreClock = clock;
    step = cycles_per_read;
    cycles = 0;
    first_read = true;
}

static __attribute__((noinline)) unsigned check_delays(void) {
    // Ten seconds at the H743 clock (one minute at the L496 clock) crosses a full revolution.
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    uint32_t const clock = 480000000U;
    uint32_t const seconds = 10U;
#else
    uint32_t const clock = 80000000U;
    uint32_t const seconds = 60U;
#endif
    uint64_t const expected = (uint64_t)clock * seconds;
    start(clock, UINT32_MAX);
    uni_hal_dwt_delay_us(seconds * 1000000U);
    CHECK(cycles >= expected && cycles - expected < step);

    // No sample is exactly UINT32_MAX cycles after the start.
    start(clock, 0x20000000U);
    uni_hal_dwt_delay_ms(seconds * 1000U);
    CHECK(cycles >= expected && cycles - expected < step);

    // Milliseconds need more than 32 bits when converted to microseconds.
    start(1000000U, 100000000U);
    uni_hal_dwt_delay_ms(5000000U);
    CHECK(cycles >= 5000000000ULL);

    // Rounding up at a fractional MHz and a wrap during a short delay.
    start(2500000U, 1U);
    uni_hal_dwt_delay_us(1U);
    CHECK(cycles == 3U);
    return 0;
}


static unsigned check(void) {
    // The test image runs without the vendor startup that normally enables the FPU.
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    __DSB();
    __ISB();
    return check_delays();
}

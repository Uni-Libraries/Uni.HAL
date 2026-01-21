//
// Includes
//

#include <stm32h7xx.h>

#include <limits.h>

// uni_hal
#include "uni_common.h"
#include "dwt/uni_hal_dwt.h"


//
// Functions
//

static uint32_t uni_hal_dwt_clamp_u64_to_u32(uint64_t value)
{
    return (value > (uint64_t)UINT32_MAX) ? UINT32_MAX : (uint32_t)value;
}

static uint32_t uni_hal_dwt_convert_ticks_scaled_clamped(uint32_t tick, uint32_t units_per_second)
{
    if (SystemCoreClock == 0U) {
        return 0U;
    }

    // tick is in CPU cycles, SystemCoreClock is cycles/second.
    // Convert via: time = tick * units_per_second / SystemCoreClock.
    const uint64_t scaled = (uint64_t)tick * (uint64_t)units_per_second;
    const uint64_t result = scaled / (uint64_t)SystemCoreClock;
    return uni_hal_dwt_clamp_u64_to_u32(result);
}

bool uni_hal_dwt_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; //-V2571
    DWT->CYCCNT = 0U; //-V2571
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; //-V2571
    return true;
}

bool uni_hal_dwt_is_inited(void) {
    uint32_t val_1 = uni_hal_dwt_get_tick();
    uint32_t val_2 = uni_hal_dwt_get_tick();
    return val_1 != val_2;
}

uint32_t uni_hal_dwt_get_tick(void) { return DWT->CYCCNT; } //-V2571

uint32_t uni_hal_dwt_get_ms(void) { return uni_hal_dwt_convert_ms(uni_hal_dwt_get_tick()); }

uint32_t uni_hal_dwt_get_us(void) { return uni_hal_dwt_convert_us(uni_hal_dwt_get_tick()); }

//
// Convert
//

uint32_t uni_hal_dwt_convert_ms(uint32_t tick) {
    return uni_hal_dwt_convert_ticks_scaled_clamped(tick, 1'000U);
}

uint32_t uni_hal_dwt_convert_ns(uint32_t tick) {
    return uni_hal_dwt_convert_ticks_scaled_clamped(tick, 1'000'000'000U);
}

uint32_t uni_hal_dwt_convert_us(uint32_t tick) {
    return uni_hal_dwt_convert_ticks_scaled_clamped(tick, 1'000'000U);
}


//
// Compare
//

uint32_t uni_hal_dwt_compare(uint32_t timestamp_1, uint32_t timestamp_2) {
    // Unsigned subtraction is modulo 2^32 and therefore naturally handles overflow.
    return (uint32_t)(timestamp_2 - timestamp_1);
}




//
// Delay
//

UNI_COMMON_COMPILER_OPTIMIZATION("O0")
void uni_hal_dwt_delay_us(uint32_t us)
{
    const uint32_t start_tick = uni_hal_dwt_get_tick();

    if (SystemCoreClock == 0U) {
        return;
    }

    // Convert requested microseconds to CPU cycles with clamping.
    // Round up so we never delay *less* than requested due to integer truncation.
    const uint64_t delay_ticks_64 = (((uint64_t)us * (uint64_t)SystemCoreClock) + (1'000'000U - 1U)) / 1'000'000U;
    const uint32_t delay_ticks = uni_hal_dwt_clamp_u64_to_u32(delay_ticks_64);

    while (uni_hal_dwt_compare(start_tick, uni_hal_dwt_get_tick()) < delay_ticks) {
    }
}

void uni_hal_dwt_delay_ms(uint32_t ms)
{
    const uint64_t us_64 = (uint64_t)ms * 1'000U;
    const uint32_t us = uni_hal_dwt_clamp_u64_to_u32(us_64);
    uni_hal_dwt_delay_us(us);
}

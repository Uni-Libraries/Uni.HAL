//
// Includes
//

#include <stm32h7xx.h>

// uni_hal
#include "uni_common.h"
#include "dwt/uni_hal_dwt.h"
#include "dwt/uni_hal_dwt_internal.h"


//
// Constants
//

enum {
    UNI_HAL_DWT_STM32H7_DEFAULT_CLOCK_HZ = 64000000U,
};


//
// Functions
//

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

//
// Convert
//

uint32_t uni_hal_dwt_convert_ms(uint32_t tick) {
    return uni_hal_dwt_internal_convert_ticks_scaled_clamped(tick, SystemCoreClock, UNI_HAL_DWT_UNITS_PER_SECOND_MS);
}

uint32_t uni_hal_dwt_convert_ns(uint32_t tick) {
    return uni_hal_dwt_internal_convert_ticks_scaled_clamped(tick, SystemCoreClock, UNI_HAL_DWT_UNITS_PER_SECOND_NS);
}

uint32_t uni_hal_dwt_convert_us(uint32_t tick) {
    return uni_hal_dwt_internal_convert_ticks_scaled_clamped(tick, SystemCoreClock, UNI_HAL_DWT_UNITS_PER_SECOND_US);
}


uint32_t uni_hal_dwt_timeout_ms_to_tick(uint32_t timeout_ms) {
    uint32_t freq_hz = SystemCoreClock;

    if (freq_hz == 0U) {
        freq_hz = UNI_HAL_DWT_STM32H7_DEFAULT_CLOCK_HZ;
    }

    return uni_hal_dwt_internal_timeout_ms_to_tick(timeout_ms, freq_hz);
}
//
// Delay
//

UNI_COMMON_COMPILER_OPTIMIZATION("O0")
void uni_hal_dwt_delay_us(uint32_t us)
{
    const uint32_t start_tick = uni_hal_dwt_get_tick();

    const uint32_t core_clock_hz = SystemCoreClock;
    if (core_clock_hz == 0U) {
        return;
    }

    // Convert requested microseconds to CPU cycles with clamping.
    // Round up so we never delay *less* than requested due to integer truncation.
    const uint32_t delay_ticks = uni_hal_dwt_internal_delay_us_to_tick_ceil(us, core_clock_hz);

    while (uni_hal_dwt_compare(start_tick, uni_hal_dwt_get_tick()) < delay_ticks) {
    }
}

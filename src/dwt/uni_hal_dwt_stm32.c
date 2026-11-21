#include "uni_hal_target.h"

#if defined(UNI_HAL_TARGET_STM32H7)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif

#include "dwt/uni_hal_dwt.h"
#include "dwt/uni_hal_dwt_internal.h"

void uni_hal_dwt_delay_us(uint32_t us)
{
    uint32_t previous = uni_hal_dwt_get_tick();
    uint64_t const scaled = (uint64_t)us * SystemCoreClock;
    uint64_t const target = (scaled + UNI_HAL_DWT_UNITS_PER_SECOND_US - 1U) / UNI_HAL_DWT_UNITS_PER_SECOND_US;

    // Each unsigned difference crosses CYCCNT zero naturally. A delay can span several revolutions.
    uint64_t elapsed = 0U;
    while (elapsed < target) {
        uint32_t const now = uni_hal_dwt_get_tick();
        elapsed += (uint32_t)(now - previous);
        previous = now;
    }
}

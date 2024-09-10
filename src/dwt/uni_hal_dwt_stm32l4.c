//
// Includes
//

// ST
#include <stm32l4xx.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "dwt/uni_hal_dwt.h"



//
// Functions
//

bool uni_hal_dwt_init() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    return true;
}

bool uni_hal_dwt_is_inited(void) {
    uint32_t val_1 = uni_hal_dwt_get_tick();
    uint32_t val_2 = uni_hal_dwt_get_tick();
    return val_1 != val_2;
}

void uni_hal_dwt_delay_ms(uint32_t ms) // microseconds
{
    uni_hal_dwt_delay_us(ms * 1000U);
}


UNI_COMMON_COMPILER_OPTIMIZATION("O0")
void uni_hal_dwt_delay_us(uint32_t us) // microseconds
{
    uint32_t startTick = DWT->CYCCNT;
    uint32_t delayTicks = us * (SystemCoreClock / 1000000U);

    while (DWT->CYCCNT - startTick < delayTicks) {
    }
}


uint32_t uni_hal_dwt_get_ms(void) { return DWT->CYCCNT / (SystemCoreClock / 1000U); }


uint32_t uni_hal_dwt_get_us(void) { return DWT->CYCCNT / (SystemCoreClock / 1000000U); }


uint32_t uni_hal_dwt_get_tick(void) { return DWT->CYCCNT; }

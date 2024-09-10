//
// Includes
//

#include <stm32h7xx.h>

// uni_hal
#include "uni_common.h"
#include "dwt/uni_hal_dwt.h"


//
// Functions
//

bool uni_hal_dwt_init() {
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

uint32_t uni_hal_dwt_get_ms(void) { return uni_hal_dwt_get_tick() / (SystemCoreClock / 1'000U); }

uint32_t uni_hal_dwt_get_us(void) { return uni_hal_dwt_get_tick() / (SystemCoreClock / 1'000'000U); }


UNI_COMMON_COMPILER_OPTIMIZATION("O0")
void uni_hal_dwt_delay_us(uint32_t us)
{
    uint32_t startTick = uni_hal_dwt_get_tick();
    uint32_t delayTicks = us * (SystemCoreClock / 1000000U);

    while ((uni_hal_dwt_get_tick() - startTick) < delayTicks) {
    }
}

void uni_hal_dwt_delay_ms(uint32_t ms)
{
    uni_hal_dwt_delay_us(ms * 1000U);
}

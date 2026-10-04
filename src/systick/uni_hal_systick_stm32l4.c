//
// Includes
//

// st
#include <stm32l4xx.h>
#include <stm32l4xx_ll_cortex.h>
#include <stm32l4xx_ll_utils.h>

// uni_hal
#include "systick/uni_hal_systick.h"



//
// Functions
//

bool uni_hal_systick_init(void) {
    // On the STM32L4 the processor clock that feeds SysTick is HCLK, which is SystemCoreClock
    SystemCoreClockUpdate();

    NVIC_SetPriority(SysTick_IRQn, 0U);
    LL_InitTick(SystemCoreClock, 1000U);
    LL_SYSTICK_EnableIT();

    return true;
}

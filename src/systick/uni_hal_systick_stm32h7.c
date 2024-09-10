//
// Includes
//

// st
#include <stm32h7xx.h>
#include <stm32h7xx_ll_cortex.h>
#include <stm32h7xx_ll_utils.h>

// uni_hal
#include "rcc/uni_hal_rcc.h"
#include "systick/uni_hal_systick.h"



//
// Functions
//

bool uni_hal_systick_init(void) {
    // setup SysTick
    if(!NVIC_GetEnableIRQ(SysTick_IRQn)) {
        NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
        NVIC_EnableIRQ(SysTick_IRQn);
    }

    LL_InitTick(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_SYSCLK), 1000U);
    LL_SYSTICK_EnableIT();

    return true;
}

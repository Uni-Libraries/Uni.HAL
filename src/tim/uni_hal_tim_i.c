//
// Includes
//

// FreeRTOS
#include <FreeRTOS.h>

// uni_hal
#include "tim/uni_hal_tim.h"



//
// IRQ Handlers
//

#if !defined(UNI_HAL_TARGET_MCU_STM32L496)
void TIM15_IRQHandler(void){
    portYIELD_FROM_ISR(uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_15));
}

void TIM16_IRQHandler(void){
    portYIELD_FROM_ISR(uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_16));
}

void TIM17_IRQHandler(void){
    portYIELD_FROM_ISR(uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_17));
}
#endif

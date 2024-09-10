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

void TIM15_IRQHandler(){
    portYIELD_FROM_ISR(uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_15));
}

void TIM16_IRQHandler(){
    portYIELD_FROM_ISR(uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_16));
}

void TIM17_IRQHandler(){
    portYIELD_FROM_ISR(uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_17));
}

//
// Includes
//

// Uni.HAL OS layer
#include "os/uni_hal_os.h"

#include <uni_common.h>

// uni_hal
#include "tim/uni_hal_tim.h"



//
// IRQ Handlers
//

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
UNI_COMMON_COMPILER_WEAK void TIM2_IRQHandler(void){
    UNI_HAL_OS_ISR_EXIT(uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_2));
}

void TIM15_IRQHandler(void){
    UNI_HAL_OS_ISR_EXIT(uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_15));
}

void TIM16_IRQHandler(void){
    UNI_HAL_OS_ISR_EXIT(uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_16));
}

void TIM17_IRQHandler(void){
    UNI_HAL_OS_ISR_EXIT(uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_17));
}
#endif

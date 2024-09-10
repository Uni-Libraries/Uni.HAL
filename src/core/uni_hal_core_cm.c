//
// Includes
//

// stdlib
#include <limits.h>

// CMSIS
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
#include <stm32l4xx.h>
#elif defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#error "Unknown MCU"
#endif

// uni_hal
#include "uni_hal_core.h"



//
// Private
//





//
// Public
//

void uni_hal_core_irq_init(void){
    NVIC_SetPriorityGrouping(0x00000003);
}


bool uni_hal_core_irq_enable(uni_hal_core_irq_e irq, uint32_t priority_group, uint32_t priority_subgroup) {
    bool result = false;
    uint32_t irq_num = uni_hal_core_irq_getnum(irq);
    if (irq_num != INT16_MAX) {
        NVIC_SetPriority(irq_num, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), priority_group, priority_subgroup));
        NVIC_EnableIRQ(irq_num);
        result = true;
    }
    return result;
}

#pragma once

//
// Includes
//

// stdlib
#include <stdint.h>

// uni_hal
#include "core/uni_hal_core_enum.h"
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    #include "core/uni_hal_core_cm7.h"
    #include "core/uni_hal_core_stm32h7.h"
#endif



//
// Functions
//

void uni_hal_core_irq_init(void);

bool uni_hal_core_irq_enable(uni_hal_core_irq_e irq, uint32_t priority_group, uint32_t priority_subgroup);

uint32_t uni_hal_core_irq_getnum(uni_hal_core_irq_e irq);

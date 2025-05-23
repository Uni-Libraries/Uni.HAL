//
// Includes
//

// st
#include <stm32l4xx.h>

// uni_hal
#include "core/uni_hal_core_enum.h"


//
// Functions
//


IRQn_Type uni_hal_core_irq_getnum(uni_hal_core_irq_e irq)
{
    uint16_t result = UINT16_MAX;

    switch(irq)
    {
    case UNI_HAL_CORE_IRQ_I2C_1_IRQ1:
        result = I2C1_EV_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_I2C_1_IRQ2:
        result = I2C1_ER_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_I2C_2_IRQ1:
        result = I2C2_EV_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_I2C_2_IRQ2:
        result = I2C2_ER_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_I2C_3_IRQ1:
        result = I2C3_EV_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_I2C_3_IRQ2:
        result = I2C3_ER_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_I2C_4_IRQ1:
        result = I2C4_EV_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_I2C_4_IRQ2:
        result = I2C4_ER_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_TIM_1:
        result = TIM1_CC_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_UART_1:
        result = USART1_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_UART_2:
        result = USART2_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_UART_3:
        result = USART3_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_UART_4:
        result = UART4_IRQn;
        break;
    case UNI_HAL_CORE_IRQ_UART_5:
        result = UART5_IRQn;
        break;
    default:
        result = INT16_MAX;
        break;
    }
    return result;
}

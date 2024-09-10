//
// Includes
//

// st
#include <stm32h7xx.h>

// uni_hal
#include "core/uni_hal_core_enum.h"
#include "core/uni_hal_core_stm32h7.h"


//
// Functions
//

IRQn_Type uni_hal_core_irq_getnum(uni_hal_core_irq_e irq) {
    IRQn_Type result;
    switch (irq) {
        case UNI_HAL_CORE_IRQ_ETH:
            result = ETH_IRQn;
        break;
        case UNI_HAL_CORE_IRQ_TIM_15:
            result = TIM15_IRQn;
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
        case UNI_HAL_CORE_IRQ_UART_6:
            result = USART6_IRQn;
        break;
        case UNI_HAL_CORE_IRQ_UART_7:
            result = UART7_IRQn;
        break;
        case UNI_HAL_CORE_IRQ_UART_8:
            result = UART8_IRQn;
        break;
        default:
            result = INT16_MAX;
        break;
    }
    return result;
}

uni_hal_core_stm32h7_revision_e uni_hal_core_stm32h7_revision_get(void){
    return ((DBGMCU->IDCODE & DBGMCU_IDCODE_REV_ID) >> 16);
}

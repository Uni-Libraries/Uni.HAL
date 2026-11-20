#pragma once
#include <stdbool.h>
#include "core/uni_hal_core_enum.h"

// USART1/6 share one kernel mux. USART2/3 and UART4/5/7/8 share the other.
static inline bool uart_uses_apb2(uni_hal_core_periph_e port) {
    return port == UNI_HAL_CORE_PERIPH_UART_1 || port == UNI_HAL_CORE_PERIPH_UART_6;
}

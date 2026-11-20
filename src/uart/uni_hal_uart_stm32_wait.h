#pragma once

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_usart.h>
#else
#include <stm32l4xx_ll_usart.h>
#endif
#include "systick/uni_hal_systick_stm32_wait.h"

static inline bool uni_hal_uart_stm32_wait_enabled(const USART_TypeDef *uart) {
    uint32_t const direction = LL_USART_GetTransferDirection(uart);
    uint32_t const flags = ((direction & LL_USART_DIRECTION_TX) ? USART_ISR_TEACK : 0U) |
                           ((direction & LL_USART_DIRECTION_RX) ? USART_ISR_REACK : 0U);
    return uni_hal_stm32_wait_bits(&uart->ISR, flags, flags, 100U);
}

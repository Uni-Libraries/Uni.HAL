#pragma once

// Run the public driver and the ST baud-rate setup against RAM-backed RCC and USART registers.
#include <stm32h7xx.h>
static RCC_TypeDef rcc;
static USART_TypeDef usart1, usart2, usart3, usart6;
#undef RCC_BASE
#define RCC_BASE ((uintptr_t)&rcc)
#undef USART1
#define USART1 (&usart1)
#undef USART2
#define USART2 (&usart2)
#undef USART3
#define USART3 (&usart3)
#undef USART6
#define USART6 (&usart6)
#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_rcc.c"
#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_usart.c"
#include "rcc/uni_hal_rcc_stm32h7.c"
#include "uart/uni_hal_uart.h"

static uni_hal_usart_context_t *preempting_port;
static bool preempting_result;
static bool model_init_pin(uni_hal_gpio_pin_context_t *pin) {
    if (preempting_port != nullptr) {
        uni_hal_usart_context_t *other = preempting_port;
        preempting_port = nullptr;
        preempting_result = uni_hal_usart_init(other);
    }
    pin->inited = true;
    return true;
}
#define uni_hal_gpio_pin_init model_init_pin
#include "uart/uni_hal_uart_stm32h7.c"

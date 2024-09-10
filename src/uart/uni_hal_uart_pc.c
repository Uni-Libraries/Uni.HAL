//
// Includes
//

#include "uart/uni_hal_uart.h"



//
// Functions
//

bool uni_hal_usart_init(uni_hal_usart_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        ctx->initialized = true;
        result = true;
    }
    return result;
}


uint32_t uni_hal_usart_baudrate_get(uni_hal_usart_context_t *ctx) {
    uint32_t result = 0U;
    if (uni_hal_uart_is_inited(ctx)) {
        result = 1U;
    }
    return result;
}


bool uni_hal_usart_baudrate_set(uni_hal_usart_context_t *ctx, uint32_t baudrate){
    bool result = false;
    if (uni_hal_uart_is_inited(ctx)) {
        (void)baudrate;
        result = true;
    }
    return result;
}


bool uni_hal_usart_receive_enable(uni_hal_usart_context_t *ctx, bool value){
    bool result = false;
    if (uni_hal_uart_is_inited(ctx)) {
        (void)value;
        result = true;
    }
    return result;
}


bool uni_hal_usart_transmit_enable(uni_hal_usart_context_t *ctx, bool value){
    bool result = false;
    if (uni_hal_uart_is_inited(ctx)) {
        (void)value;
        result = true;
    }
    return result;
}

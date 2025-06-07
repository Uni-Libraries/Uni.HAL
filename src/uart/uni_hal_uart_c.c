//
// Includes
//

#include "uart/uni_hal_uart.h"



//
// Functions
//

bool uni_hal_uart_is_inited(const uni_hal_usart_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = ctx->initialized;
    }
    return result;
}


uni_hal_io_context_t* uni_hal_uart_io_get(uni_hal_usart_context_t *ctx) {
    uni_hal_io_context_t *result = NULL;
    if (ctx != NULL) {
        result = ctx->io;
    }
    return result;
}

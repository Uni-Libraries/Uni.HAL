//
// Includes
//

// uni_common
#include <uni_common.h>

// uni_hal
#include "can/uni_hal_can.h"



//
// Functions
//

uint32_t uni_hal_can_is_available(const uni_hal_can_context_t *ctx) {
    uint32_t result = 0;
    if (ctx != NULL) {
        result = uni_common_ringbuffer_length(ctx->config.buffer_rx);
    }

    return result;
}


bool uni_hal_can_is_inited(const uni_hal_can_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = ctx->status.inited;
    }
    return result;
}


bool uni_hal_can_receive(uni_hal_can_context_t *ctx, uni_hal_can_msg_t *msg) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx) && msg != NULL) {
        result = uni_common_ringbuffer_pop(ctx->config.buffer_rx, (uint8_t *)msg, 1U);
    }

    return result;
}

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
#if defined(UNI_HAL_CAN_USE_FREERTOS)
        result = uxQueueMessagesWaiting(ctx->status.queue_rx);
#else
        result = uni_common_ringbuffer_length(ctx->config.buffer_rx);
#endif
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


bool uni_hal_can_receive(uni_hal_can_context_t *ctx, uni_hal_can_msg_t *msg, size_t timeout_ms) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx) && msg != NULL) {
#if defined(UNI_HAL_CAN_USE_FREERTOS)
        result = xQueueReceive(ctx->status.queue_rx, msg, timeout_ms == portMAX_DELAY ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms)) == pdPASS;
#else
        result = uni_common_ringbuffer_pop(ctx->config.buffer_rx, (uint8_t *)msg, 1U);
#endif
    }

    return result;
}

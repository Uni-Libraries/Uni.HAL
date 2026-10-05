//
// Includes
//

#include "can/uni_hal_can.h"



//
// Functions
//


bool uni_hal_can_init(uni_hal_can_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
#if defined(UNI_HAL_CAN_USE_FREERTOS)
        ctx->status.queue_rx = xQueueCreate(UNI_HAL_CAN_QUEUE_SIZE, sizeof(uni_hal_can_msg_t));
        result = result && ctx->status.queue_rx != NULL;
#else
        result =
            result && uni_common_ringbuffer_init(ctx->config.buffer_rx, ctx->config.buffer_rx->data, ctx->config.buffer_rx->size_object, ctx->config.buffer_rx->size_total);
#endif
    }

    return result;
}


bool uni_hal_can_start(uni_hal_can_context_t *ctx){
    bool result = false;
    if(uni_hal_can_is_inited(ctx)){
        result = true;
    }
    return result;
}


bool uni_hal_can_stop(uni_hal_can_context_t *ctx){
    bool result = false;
    if(uni_hal_can_is_inited(ctx)){
        result = true;
    }
    return result;
}

bool uni_hal_can_set_filter(uni_hal_can_context_t *ctx, uint32_t fifo_num, uint32_t slot_idx, uint32_t filter_id,
                           uint32_t filter_mask){
    bool result = false;
    if(uni_hal_can_is_inited(ctx)){
        (void)fifo_num;
        (void)slot_idx;
        (void)filter_id;
        (void)filter_mask;
        result = true;
    }
    return result;
}


bool uni_hal_can_transmit(uni_hal_can_context_t *ctx, uni_hal_can_msg_t *msg){
    bool result = false;
    if(uni_hal_can_is_inited(ctx)){
        (void)msg;
        result = true;
    }
    return result;
}


bool uni_hal_can_transmit_nowait(uni_hal_can_context_t *ctx, const uni_hal_can_msg_t *msg) {
    (void)msg;
    return uni_hal_can_is_inited(ctx);
}


uint32_t uni_hal_can_transmit_free(const uni_hal_can_context_t *ctx) {
    return uni_hal_can_is_inited(ctx) ? 3U : 0U;
}


bool uni_hal_can_transmit_abort(uni_hal_can_context_t *ctx) {
    return uni_hal_can_is_inited(ctx);
}


bool uni_hal_can_set_tx_callback(uni_hal_can_context_t *ctx, uni_hal_can_tx_callback_t callback, void *cookie) {
    bool result = false;
    if (uni_hal_can_is_inited(ctx)) {
        // kept for the caller to inspect; nothing transmits on the host, so it is never called
        ctx->status.tx_callback = callback;
        ctx->status.tx_callback_cookie = cookie;
        result = true;
    }
    return result;
}

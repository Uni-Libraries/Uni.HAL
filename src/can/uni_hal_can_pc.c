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
        result = uni_common_ringbuffer_init(ctx->config.buffer_rx, ctx->config.buffer_rx->data, ctx->config.buffer_rx->size_object, ctx->config.buffer_rx->size_total);
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

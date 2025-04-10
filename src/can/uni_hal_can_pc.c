//
// Includes
//

#include "can/bsusat_hal_can.h"



//
// Functions
//


bool bsusat_can_init(bsusat_can_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = bsusat_ringbuffer_init(ctx->buffer_rx, ctx->buffer_rx->data, ctx->buffer_rx->size_object, ctx->buffer_rx->size_total);
    }

    return result;
}


bool bsusat_can_start(bsusat_can_context_t *ctx){
    bool result = false;
    if(bsusat_can_is_inited(ctx)){
        result = true;
    }
    return result;
}


bool bsusat_can_stop(bsusat_can_context_t *ctx){
    bool result = false;
    if(bsusat_can_is_inited(ctx)){
        result = true;
    }
    return result;
}

bool bsusat_can_set_filter(bsusat_can_context_t *ctx, uint32_t fifo_num, uint32_t slot_idx, uint32_t filter_id,
                           uint32_t filter_mask){
    bool result = false;
    if(bsusat_can_is_inited(ctx)){
        (void)fifo_num;
        (void)slot_idx;
        (void)filter_id;
        (void)filter_mask;
        result = true;
    }
    return result;
}


bool bsusat_can_transmit(bsusat_can_context_t *ctx, bsusat_can_msg_t *msg){
    bool result = false;
    if(bsusat_can_is_inited(ctx)){
        (void)msg;
        result = true;
    }
    return result;
}

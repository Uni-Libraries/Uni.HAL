//
// Includes
//

#include "uni_hal_io_tunnel.h"

//
// Public
//

bool uni_hal_io_tunnel_init(uni_hal_io_tunnel_context_t *ctx, uni_hal_io_context_t *io_first,
                           uni_hal_io_context_t *io_second, bool transparent) {
    bool result = false;

    if (ctx != NULL) {
        if (ctx->io_first != NULL) {
            ctx->io_first->handlers.tunnel_ctx = NULL;
        }

        if (ctx->io_second != NULL) {
            ctx->io_second->handlers.tunnel_ctx = NULL;
        }

        ctx->io_first = io_first;
        ctx->io_second = io_second;
        ctx->transparent = transparent;

        ctx->io_first->handlers.tunnel_ctx = ctx;
        ctx->io_second->handlers.tunnel_ctx = ctx;

        result = true;
    }

    return result;
}


bool bsuat_io_tunnel_is_inited(const uni_hal_io_tunnel_context_t *ctx) {
    bool result = false;
    if(ctx != NULL && ctx->io_first != NULL && ctx->io_second != NULL){
        result = true;
    }
    return result;
}


uni_hal_io_tunnel_transmit_result_t uni_hal_io_tunnel_transmit(uni_hal_io_tunnel_context_t *ctx,
                                                             uni_hal_io_context_t *io_from, uint8_t *data,
                                                             size_t data_len) {
    uni_hal_io_tunnel_transmit_result_t result = UNI_HAL_IO_TUNNEL_FAIL;

    if (ctx != NULL) {
        // select the other IO
        uni_hal_io_context_t *io_to = NULL;
        if (ctx->io_first == io_from) {
            io_to = ctx->io_second;
        } else if (ctx->io_second == io_from) {
            io_to = ctx->io_first;
        }

        // send to other io
        result = uni_hal_io_transmit_data(io_to, data, data_len) ? UNI_HAL_IO_TUNNEL_OK : UNI_HAL_IO_TUNNEL_FAIL;

        // check for transparent
        if (result == UNI_HAL_IO_TUNNEL_OK && ctx->transparent) {
            result = UNI_HAL_IO_TUNNEL_TRANSPARENT;
        }
    }

    return result;
}

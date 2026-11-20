//
// Includes
//

#include "uni_hal_io_tunnel.h"

#include "core/uni_hal_core.h"

static bool endpoint_available(const uni_hal_io_context_t *io, const uni_hal_io_tunnel_context_t *ctx) {
    return !io || !io->handlers.tunnel_ctx || io->handlers.tunnel_ctx == ctx;
}


//
// Public
//

bool uni_hal_io_tunnel_init(uni_hal_io_tunnel_context_t *ctx, uni_hal_io_context_t *io_first,
                           uni_hal_io_context_t *io_second, bool transparent) {
    if (!ctx || ((io_first == NULL) != (io_second == NULL))) {
        return false;
    }

    // UART receive interrupts must see a complete binding when the tunnel changes.
#if defined(UNI_HAL_TARGET_MCU_PC)
    taskENTER_CRITICAL();
#else
    uint32_t const mask = uni_hal_core_irq_pause();
#endif
    bool const available = endpoint_available(io_first, ctx) && endpoint_available(io_second, ctx);
    if (available) {
        if (ctx->io_first && ctx->io_first->handlers.tunnel_ctx == ctx) {
            ctx->io_first->handlers.tunnel_ctx = NULL;
        }
        if (ctx->io_second && ctx->io_second->handlers.tunnel_ctx == ctx) {
            ctx->io_second->handlers.tunnel_ctx = NULL;
        }

        ctx->io_first = io_first;
        ctx->io_second = io_second;
        ctx->transparent = transparent;
        if (io_first) {
            io_first->handlers.tunnel_ctx = ctx;
            io_second->handlers.tunnel_ctx = ctx;
        }
    }
#if defined(UNI_HAL_TARGET_MCU_PC)
    taskEXIT_CRITICAL();
#else
    uni_hal_core_irq_resume(mask);
#endif
    return available;
}


bool uni_hal_io_tunnel_is_inited(const uni_hal_io_tunnel_context_t *ctx) {
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

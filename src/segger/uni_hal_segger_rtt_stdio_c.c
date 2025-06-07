//
// Includes
//

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "io/uni_hal_io_tunnel.h"
#include "tim/uni_hal_tim.h"
#include "segger/uni_hal_segger_rtt.h"
#include "segger/uni_hal_segger_rtt_stdio.h"

#if !defined(UNI_HAL_TARGET_MCU_PC)

// segger
#include "SEGGER_RTT.h"




//
// Timer handlers
//

bool _uni_hal_segger_rtt_stdio_timer(void *UNI_COMMON_COMPILER_UNUSED_VAR(ctx_timer_r), void *ctx_fn_r) {
    BaseType_t higher_task_woken = false;

    uint8_t data = 0U;

    uni_hal_segger_context_t *ctx_fn = (uni_hal_segger_context_t *)ctx_fn_r;
    if (ctx_fn != NULL) {
        uni_hal_io_context_t *ctx_io = ctx_fn->io;
        if (ctx_io != NULL) {
            // RX
            while (SEGGER_RTT_HasData(ctx_fn->buffer_index_down)) {
                // receive data
                SEGGER_RTT_Read(ctx_fn->buffer_index_down, &data, sizeof(data));

                // try to tunnel data
                uni_hal_io_tunnel_transmit_result_t tunnel_result =
                        uni_hal_io_tunnel_transmit(ctx_io->handlers.tunnel_ctx, ctx_io, &data, sizeof(data));

                // send to our ringbuffer in case we failed to tunnel data
                if (tunnel_result == UNI_HAL_IO_TUNNEL_FAIL || tunnel_result == UNI_HAL_IO_TUNNEL_TRANSPARENT) {
                    if (xStreamBufferSendFromISR(ctx_io->buf_rx.handle, &data, 1U, &higher_task_woken)) {
                        ctx_io->stats.rx_received++;
                    }
                    else {
                        ctx_io->stats.rx_overrun++;
                    }
                }
            }

            // TX
            while (xStreamBufferReceiveFromISR(ctx_io->buf_tx.handle, &data, 1U, &higher_task_woken)) {
                SEGGER_RTT_PutChar(ctx_fn->buffer_index_up, data);
                ctx_io->stats.tx_transmited++;
            }
        }
    }

    return higher_task_woken;
}

#endif

//
// Functions
//

bool uni_hal_segger_rtt_stdio_init(uni_hal_segger_context_t *ctx) {
#if !defined(UNI_HAL_TARGET_MCU_PC)
    bool result = false;

    if (ctx && !ctx->initialized && ctx->io /*&& ctx->timer*/) {
        uni_hal_segger_rtt_init();

        uni_hal_io_init(ctx->io);

        uni_hal_tim_init(ctx->timer);
        uni_hal_tim_register_callback(ctx->timer, _uni_hal_segger_rtt_stdio_timer, ctx);
        uni_hal_tim_start(ctx->timer);

        ctx->initialized = true;

        result = true;
    }

    return result;
#else
    (void)ctx;
    return false;
#endif
}


bool uni_hal_segger_rtt_stdio_is_inited(const uni_hal_segger_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = ctx->initialized;
    }
    return result;
}

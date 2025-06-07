//
// Includes
//

// stdlib
#include <string.h>
#include <time.h>

// uni_hal
#include "io/uni_hal_io.h"
#include "systick/uni_hal_systick.h"


//
// Functions/Init
//

bool uni_hal_io_init(uni_hal_io_context_t *ctx) {
    bool result = false;

    if (ctx != NULL) {
        (void) memset(&ctx->handlers, 0, sizeof(uni_hal_io_handlers_t));
        (void) memset(&ctx->stats, 0, sizeof(uni_hal_io_stats_t));

        ctx->buf_rx.handle = xStreamBufferCreateStatic(ctx->buf_rx.size, 1U, ctx->buf_rx.array, &ctx->buf_rx.cb);
        ctx->buf_tx.handle = xStreamBufferCreateStatic(ctx->buf_tx.size, 1U, ctx->buf_tx.array, &ctx->buf_tx.cb);

        if (ctx->buf_rx.handle != NULL && ctx->buf_tx.handle != NULL) {
            result = true;
        }
    }

    return result;
}


//
// Receive
//

size_t uni_hal_io_receive_available(const uni_hal_io_context_t *ctx) {
    size_t result = 0;
    if (ctx != NULL) {
        result = xStreamBufferBytesAvailable(ctx->buf_rx.handle);
    }
    return result;
}


bool uni_hal_io_receive_clear(uni_hal_io_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = xStreamBufferReset(ctx->buf_rx.handle);
    }
    return result;
}


size_t uni_hal_io_receive_data(uni_hal_io_context_t *ctx, uint8_t *data, uint32_t data_len, uint32_t timeout) {
    size_t received = 0;

    if (ctx != NULL) {
        size_t ticktime = uni_hal_systick_get_ms();

        do {
            received += xStreamBufferReceive(ctx->buf_rx.handle, &data[received], data_len - received, 0U);
        } while (received < data_len && uni_hal_systick_get_ms() - ticktime < timeout);
    }

    return received;
}


//
// Functions/Transmit
//


size_t uni_hal_io_transmit_data(uni_hal_io_context_t *ctx, const uint8_t *data, uint32_t data_len) {
    size_t result = 0;

    if (ctx != NULL && ctx->buf_tx.handle != NULL && data != NULL) {
        // push to buffer
        result = xStreamBufferSend(ctx->buf_tx.handle, data, data_len, 0U);

        // call TX trigger hook
        if (ctx->handlers.tx_trigger != NULL) {
            ctx->handlers.tx_trigger(ctx, ctx->handlers.tx_trigger_ctx);
        }
    }

    return result;
}

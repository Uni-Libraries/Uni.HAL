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

        // reset sync progress
        ctx->buf_rx.sync_idx = 0U;
        ctx->buf_tx.sync_idx = 0U;

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
        // reset sync progress and buffer
        ctx->buf_rx.sync_idx = 0U;
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


bool uni_hal_io_receive_sync(uni_hal_io_context_t *ctx, const uint8_t *data, size_t data_len, uint32_t timeout) {
    // Validate inputs
    if (ctx == NULL || ctx->buf_rx.handle == NULL || data == NULL || data_len == 0U) {
        return false;
    }

    const size_t pat_len = data_len;
    size_t match_idx = ctx->buf_rx.sync_idx; // resume progress across calls, if any
    uint32_t const t_start = uni_hal_systick_get_ms();

    for (;;) {
        // Block using FreeRTOS ticks to avoid busy-waiting
        TickType_t ticks_to_wait = 0;
        if (timeout > 0U) {
            uint32_t const now = uni_hal_systick_get_ms();
            uint32_t const elapsed = now - t_start;
            if (elapsed >= timeout) {
                // Timeout: drain RX and persist current match progress, leave buffer empty
                uint8_t dump[32];
                while (xStreamBufferReceive(ctx->buf_rx.handle, dump, sizeof(dump), 0U) > 0U) { }
                ctx->buf_rx.sync_idx = match_idx;
                return false;
            }
            uint32_t const rem_ms = timeout - elapsed;
            ticks_to_wait = pdMS_TO_TICKS(rem_ms);
            if ((ticks_to_wait == 0U) && (rem_ms > 0U)) {
                ticks_to_wait = 1U; // ensure at least one tick to yield
            }
        }

        uint8_t ch = 0U;
        size_t const got = xStreamBufferReceive(ctx->buf_rx.handle, &ch, 1U, ticks_to_wait);

        if (got == 0U) {
            if (timeout == 0U) {
                // Immediate mode: drain and store progress, leave buffer empty
                uint8_t dump[32];
                while (xStreamBufferReceive(ctx->buf_rx.handle, dump, sizeof(dump), 0U) > 0U) { }
                ctx->buf_rx.sync_idx = match_idx;
                return false;
            }
            // Still time remaining, try again
            continue;
        }

        // Advance simple prefix matcher using only an index (no VLA/extra buffers)
        if (ch == data[match_idx]) {
            match_idx++;
            if (match_idx == pat_len) {
                // Pattern found. Do not reconstruct/alter remaining RX content; stop here.
                ctx->buf_rx.sync_idx = 0U;
                return true;
            }
        } else {
            // Simple overlap handling without tables: if current char equals first pattern byte,
            // keep 1, else reset to 0.
            match_idx = (ch == data[0]) ? 1U : 0U;
        }
    }
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

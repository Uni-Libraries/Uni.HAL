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


bool uni_hal_io_receive_sync(uni_hal_io_context_t *ctx, const uint8_t *data, size_t data_len, uint32_t timeout) {
    // Validate inputs
    if (ctx == NULL || ctx->buf_rx.handle == NULL || data == NULL || data_len == 0U) {
        return false;
    }

    const size_t pat_len = data_len;
    size_t match_idx = 0U; // number of contiguous matched bytes
    bool found = false;

    uint32_t const t_start = uni_hal_systick_get_ms();

    for (;;) {
        // Determine how long to wait inside FreeRTOS call
        TickType_t ticks_to_wait = 0;
        if (timeout > 0U) {
            uint32_t const now = uni_hal_systick_get_ms();
            uint32_t const elapsed = now - t_start;
            if (elapsed >= timeout) {
                break;
            }
            uint32_t rem_ms = timeout - elapsed;
            ticks_to_wait = pdMS_TO_TICKS(rem_ms);
            if ((ticks_to_wait == 0U) && (rem_ms > 0U)) {
                // ensure at least one tick wait to avoid busy loop
                ticks_to_wait = 1U;
            }
        }

        uint8_t ch = 0U;
        size_t const got = xStreamBufferReceive(ctx->buf_rx.handle, &ch, 1U, ticks_to_wait);

        if (got == 0U) {
            // no data now
            if (timeout == 0U) {
                break;
            }
            // still have time: try again
            continue;
        }

        // Update match index without any VLA/window
        if (ch == data[match_idx]) {
            match_idx++;
            if (match_idx == pat_len) {
                found = true;
                break;
            }
        } else {
            // naive fallback without LPS table: keep only immediate prefix if possible
            if (match_idx > 0U) {
                match_idx = (ch == data[0]) ? 1U : 0U;
            } else {
                match_idx = (ch == data[0]) ? 1U : 0U;
            }
        }
    }

    if (found) {
        // Tail remaining in RX after the matched pattern
        size_t tail_len = xStreamBufferBytesAvailable(ctx->buf_rx.handle);
        uint8_t *tail = NULL;

        if (tail_len > 0U) {
            tail = (uint8_t *) pvPortMalloc(tail_len);
            if (tail != NULL) {
                size_t rd = 0U;
                while (rd < tail_len) {
                    size_t const g = xStreamBufferReceive(ctx->buf_rx.handle, &tail[rd], tail_len - rd, 0U);
                    if (g == 0U) {
                        break;
                    }
                    rd += g;
                }
                tail_len = rd;
            } else {
                // Allocation failed: drain to discard to keep consistent state
                (void) xStreamBufferReceive(ctx->buf_rx.handle, NULL, 0U, 0U);
                tail_len = 0U;
            }
        }

        // Rebuild RX buffer: pattern + tail
        (void) xStreamBufferReset(ctx->buf_rx.handle);

        // Write pattern first
        size_t sent = 0U;
        while (sent < pat_len) {
            size_t const s = xStreamBufferSend(ctx->buf_rx.handle, &data[sent], pat_len - sent, 0U);
            if (s == 0U) {
                break;
            }
            sent += s;
        }

        // Then append tail if buffered
        if (tail != NULL) {
            size_t ts = 0U;
            while (ts < tail_len) {
                size_t const s = xStreamBufferSend(ctx->buf_rx.handle, &tail[ts], tail_len - ts, 0U);
                if (s == 0U) {
                    break;
                }
                ts += s;
            }
            vPortFree(tail);
        }

        return true;
    } else {
        // Keep partial prefix equal to matched length
        size_t keep = match_idx;
        size_t sent = 0U;
        while (sent < keep) {
            size_t const s = xStreamBufferSend(ctx->buf_rx.handle, &data[sent], keep - sent, 0U);
            if (s == 0U) {
                break;
            }
            sent += s;
        }
        return false;
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

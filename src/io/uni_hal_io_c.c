//
// Includes
//

// stdlib
#include <string.h>
#include <time.h>

// FreeRTOS
#include <FreeRTOS.h>
#include <task.h>

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

    if (ctx != NULL && ctx->buf_rx.handle != NULL && data != NULL) {
        uint32_t const t_start = uni_hal_systick_get_ms();

        // A task sleeps until data arrives. Before the scheduler runs and inside an interrupt
        // there is nothing to switch to, so the buffer is polled there.
        bool can_block = xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
#if !defined(UNI_HAL_TARGET_MCU_PC)
        can_block = can_block && !xPortIsInsideInterrupt();
#endif

        for (;;) {
            uint32_t const elapsed = uni_hal_systick_get_ms() - t_start;

            TickType_t ticks_to_wait = 0U;
            if (can_block && elapsed < timeout) {
                ticks_to_wait = pdMS_TO_TICKS(timeout - elapsed);
                if (ticks_to_wait == 0U) {
                    ticks_to_wait = 1U;
                }
            }

            received += xStreamBufferReceive(ctx->buf_rx.handle, &data[received], data_len - received, ticks_to_wait);

            if (received >= data_len || (uni_hal_systick_get_ms() - t_start) >= timeout) {
                break;
            }
        }
    }

    return received;
}


bool uni_hal_io_receive_sync(uni_hal_io_context_t *ctx, const uint8_t *data, size_t data_len, uint32_t timeout) {
    // Validate inputs
    if (ctx == NULL || ctx->buf_rx.handle == NULL || data == NULL || data_len == 0U) {
        return false;
    }

    const size_t pat_len = data_len;
    size_t match_idx = ctx->buf_rx.sync_idx; // resume progress across calls
    uint32_t const t_start = uni_hal_systick_get_ms();

    for (;;) {
        TickType_t ticks_to_wait = 0;
        if (timeout > 0U) {
            uint32_t const now = uni_hal_systick_get_ms();
            uint32_t const elapsed = now - t_start;
            if (elapsed >= timeout) {
                ctx->buf_rx.sync_idx = match_idx;
                return false;
            }
            uint32_t const rem_ms = timeout - elapsed;
            ticks_to_wait = pdMS_TO_TICKS(rem_ms);
            if ((ticks_to_wait == 0U) && (rem_ms > 0U)) {
                ticks_to_wait = 1U;
            }
        }

        uint8_t ch = 0U;
        size_t const got = xStreamBufferReceive(ctx->buf_rx.handle, &ch, 1U, ticks_to_wait);

        if (got == 0U) {
            // No new data available.
            ctx->buf_rx.sync_idx = match_idx;
            return false;
        }

        if (ch == data[match_idx]) {
            match_idx++;
            if (match_idx == pat_len) {
                ctx->buf_rx.sync_idx = 0U;
                return true;
            }
        } else {
            // Overlap handling: continue from the longest prefix of the pattern that the bytes
            // received so far (the matched part plus this byte) still end with.
            size_t next_idx = 0U;
            for (size_t keep = match_idx; keep > 0U; keep--) {
                if (ch == data[keep - 1U] && memcmp(data, &data[match_idx - (keep - 1U)], keep - 1U) == 0) {
                    next_idx = keep;
                    break;
                }
            }
            match_idx = next_idx;
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
        // A stream buffer supports a single writer. Several tasks and interrupts write here
        // (stdio output for one), so each write runs in a critical section with no block time.
#if defined(UNI_HAL_TARGET_MCU_PC)
        taskENTER_CRITICAL();
        result = xStreamBufferSend(ctx->buf_tx.handle, data, data_len, 0U);
        taskEXIT_CRITICAL();
#else
        if (xPortIsInsideInterrupt()) {
            UBaseType_t const saved_mask = taskENTER_CRITICAL_FROM_ISR();
            result = xStreamBufferSendFromISR(ctx->buf_tx.handle, data, data_len, nullptr);
            taskEXIT_CRITICAL_FROM_ISR(saved_mask);
        } else {
            taskENTER_CRITICAL();
            result = xStreamBufferSend(ctx->buf_tx.handle, data, data_len, 0U);
            taskEXIT_CRITICAL();
        }
#endif

        // call TX trigger hook
        if (ctx->handlers.tx_trigger != NULL) {
            ctx->handlers.tx_trigger(ctx, ctx->handlers.tx_trigger_ctx);
        }
    }

    return result;
}

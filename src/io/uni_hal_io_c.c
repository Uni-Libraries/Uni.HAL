//
// Includes
//

// stdlib
#include <string.h>

// uni_hal
#include "io/uni_hal_io.h"
#include "systick/uni_hal_systick.h"


//
// Private
//

static bool _uni_hal_io_buf_create(uni_hal_io_buffer_t *buf) {
#if configSUPPORT_STATIC_ALLOCATION
    if (buf->size == 0U || buf->array == nullptr) {
        return false;
    }
    buf->handle = xStreamBufferCreateStatic(buf->size, 1U, buf->array, &buf->cb);
#else
    if (buf->size < 2U) {
        return false;
    }
    // FreeRTOS adds the reserved slot itself in its dynamic constructor.
    buf->handle = xStreamBufferCreate(buf->size - 1U, 1U);
#endif
    return buf->handle != nullptr;
}

/**
 * Take bytes out of a buffer, task context
 * @param wait_ms how long to sleep for the first byte when the buffer is empty
 */
static size_t _uni_hal_io_buf_read(uni_hal_io_buffer_t *buf, uint8_t *data, size_t data_len, uint32_t wait_ms) {
    // A task sleeps until data arrives. Before the scheduler runs and inside an interrupt
    // there is nothing to switch to, so the buffer is polled there.
    bool can_block = xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
#if !defined(UNI_HAL_TARGET_MCU_PC)
    can_block = can_block && !xPortIsInsideInterrupt();
#endif

    TickType_t ticks_to_wait = 0U;
    if (can_block && wait_ms > 0U) {
        ticks_to_wait = pdMS_TO_TICKS(wait_ms);
        if (ticks_to_wait == 0U) {
            ticks_to_wait = 1U;
        }
    }

    return xStreamBufferReceive(buf->handle, data, data_len, ticks_to_wait);
}

/**
 * Store bytes in a buffer, any context
 */
static size_t _uni_hal_io_buf_write(uni_hal_io_buffer_t *buf, const uint8_t *data, size_t data_len) {
    size_t result;

    // A stream buffer supports a single writer. Several tasks and interrupts write here
    // (stdio output for one), so each write runs in a critical section with no block time.
#if defined(UNI_HAL_TARGET_MCU_PC)
    taskENTER_CRITICAL();
    result = xStreamBufferSend(buf->handle, data, data_len, 0U);
    taskEXIT_CRITICAL();
#else
    if (xPortIsInsideInterrupt()) {
        UBaseType_t const saved_mask = taskENTER_CRITICAL_FROM_ISR();
        result = xStreamBufferSendFromISR(buf->handle, data, data_len, nullptr);
        taskEXIT_CRITICAL_FROM_ISR(saved_mask);
    } else {
        taskENTER_CRITICAL();
        result = xStreamBufferSend(buf->handle, data, data_len, 0U);
        taskEXIT_CRITICAL();
    }
#endif

    return result;
}


//
// Functions/Init
//

bool uni_hal_io_init(uni_hal_io_context_t *ctx) {
    if (ctx == nullptr) {
        return false;
    }
    uni_hal_io_deinit(ctx);
    if (!_uni_hal_io_buf_create(&ctx->buf_rx) || !_uni_hal_io_buf_create(&ctx->buf_tx)) {
        uni_hal_io_deinit(ctx);
        return false;
    }
    return true;
}

void uni_hal_io_deinit(uni_hal_io_context_t *ctx) {
    if (ctx == nullptr) {
        return;
    }
#if !configSUPPORT_STATIC_ALLOCATION
    if (ctx->buf_rx.handle != nullptr) {
        vStreamBufferDelete(ctx->buf_rx.handle);
    }
    if (ctx->buf_tx.handle != nullptr) {
        vStreamBufferDelete(ctx->buf_tx.handle);
    }
#endif
    ctx->buf_rx.handle = nullptr;
    ctx->buf_tx.handle = nullptr;
    uni_hal_io_receive_sync_reset(ctx);
    ctx->buf_tx.sync_idx = 0U;
    ctx->handlers = (uni_hal_io_handlers_t){0};
    ctx->stats = (uni_hal_io_stats_t){0};
}


//
// Functions/Buffer
//

size_t uni_hal_io_buffer_push_isr(uni_hal_io_buffer_t *buf, const uint8_t *data, size_t data_len, BaseType_t *woken) {
    size_t result = 0U;
    if (buf != nullptr && buf->handle != nullptr && data != nullptr) {
        result = xStreamBufferSendFromISR(buf->handle, data, data_len, woken);
    }
    return result;
}


size_t uni_hal_io_buffer_pop_isr(uni_hal_io_buffer_t *buf, uint8_t *data, size_t data_len, BaseType_t *woken) {
    size_t result = 0U;
    if (buf != nullptr && buf->handle != nullptr && data != nullptr) {
        result = xStreamBufferReceiveFromISR(buf->handle, data, data_len, woken);
        if (result != 0U) {
            buf->sync_idx = 0U;
        }
    }
    return result;
}


bool uni_hal_io_buffer_is_empty(const uni_hal_io_buffer_t *buf) {
    return buf == nullptr || buf->handle == nullptr || xStreamBufferBytesAvailable(buf->handle) == 0U;
}


//
// Functions/Receive
//

size_t uni_hal_io_receive_available(const uni_hal_io_context_t *ctx) {
    size_t result = 0;
    if (ctx != nullptr && ctx->buf_rx.handle != nullptr) {
        result = xStreamBufferBytesAvailable(ctx->buf_rx.handle);
    }
    return result;
}


bool uni_hal_io_receive_clear(uni_hal_io_context_t *ctx) {
    bool result = false;
    if (ctx != nullptr && ctx->buf_rx.handle != nullptr) {
        result = xStreamBufferReset(ctx->buf_rx.handle) == pdPASS;
        if (result) {
            uni_hal_io_receive_sync_reset(ctx);
        }
    }
    return result;
}


size_t uni_hal_io_receive_data(uni_hal_io_context_t *ctx, uint8_t *data, uint32_t data_len, uint32_t timeout) {
    size_t received = 0;

    if (ctx != nullptr && ctx->buf_rx.handle != nullptr && data != nullptr) {
        uint32_t const t_start = uni_hal_systick_get_ms();

        for (;;) {
            uint32_t const elapsed = uni_hal_systick_get_ms() - t_start;
            uint32_t const wait_ms = (elapsed < timeout) ? (timeout - elapsed) : 0U;

            received += _uni_hal_io_buf_read(&ctx->buf_rx, &data[received], data_len - received, wait_ms);

            if (received >= data_len || (uni_hal_systick_get_ms() - t_start) >= timeout) {
                break;
            }
        }
    }

    if (received != 0U) {
        uni_hal_io_receive_sync_reset(ctx);
    }
    return received;
}


void uni_hal_io_receive_sync_reset(uni_hal_io_context_t *ctx) {
    if (ctx != nullptr) {
        ctx->buf_rx.sync_idx = 0U;
        ctx->sync_pattern_addr = 0U;
        ctx->sync_pattern_len = 0U;
    }
}


bool uni_hal_io_receive_sync(uni_hal_io_context_t *ctx, const uint8_t *data, size_t data_len, uint32_t timeout) {
    // Validate inputs
    if (ctx == nullptr || ctx->buf_rx.handle == nullptr || data == nullptr || data_len == 0U) {
        return false;
    }

    size_t match_idx = 0U;
    if (ctx->sync_pattern_addr == (uintptr_t)data && ctx->sync_pattern_len == data_len &&
        ctx->buf_rx.sync_idx <= data_len) {
        match_idx = ctx->buf_rx.sync_idx;
    }
    if (match_idx == data_len) {
        uni_hal_io_receive_sync_reset(ctx);
        return true;
    }

    uint32_t const start = timeout != 0U ? uni_hal_systick_get_ms() : 0U;
    size_t available = timeout == 0U ? xStreamBufferBytesAvailable(ctx->buf_rx.handle) : 0U;
    for (;;) {
        uint32_t wait_ms = 0U;
        if (timeout != 0U) {
            uint32_t const elapsed = uni_hal_systick_get_ms() - start;
            if (elapsed >= timeout) {
                break;
            }
            wait_ms = timeout - elapsed;
        } else {
            if (available == 0U) {
                break;
            }
            --available;
        }

        uint8_t ch;
        if (_uni_hal_io_buf_read(&ctx->buf_rx, &ch, 1U, wait_ms) == 0U) {
            if (timeout == 0U) {
                break;
            }
            continue;
        }

        if (ch == data[match_idx]) {
            match_idx++;
            if (match_idx == data_len) {
                if (timeout != 0U && (uni_hal_systick_get_ms() - start) > timeout) {
                    // The read consumed the marker. Deliver it on the next matching call.
                    break;
                }
                uni_hal_io_receive_sync_reset(ctx);
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

    ctx->buf_rx.sync_idx = match_idx;
    ctx->sync_pattern_addr = (uintptr_t)data;
    ctx->sync_pattern_len = data_len;
    return false;
}



//
// Functions/Transmit
//

size_t uni_hal_io_transmit_data(uni_hal_io_context_t *ctx, const uint8_t *data, uint32_t data_len) {
    size_t result = 0;

    if (ctx != nullptr && ctx->buf_tx.handle != nullptr && data != nullptr) {
        // push to buffer
        result = _uni_hal_io_buf_write(&ctx->buf_tx, data, data_len);

        // call TX trigger hook
        if (ctx->handlers.tx_trigger != nullptr) {
            ctx->handlers.tx_trigger(ctx, ctx->handlers.tx_trigger_ctx);
        }
    }

    return result;
}

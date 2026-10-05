//
// Includes
//

// stdlib
#include <string.h>
#include <time.h>

// uni_hal
#include "core/uni_hal_core.h"
#include "io/uni_hal_io.h"
#include "os/uni_hal_os.h"
#include "systick/uni_hal_systick.h"


//
// Private
//
// The buffers are FreeRTOS stream buffers when the library is built on the RTOS and
// Uni.Common ring buffers otherwise. Everything below this section is the same for both.
//

#if defined(UNI_HAL_USE_FREERTOS)

static bool _uni_hal_io_buf_create(uni_hal_io_buffer_t *buf) {
    buf->handle = xStreamBufferCreateStatic(buf->size, 1U, buf->array, &buf->cb);
    return buf->handle != nullptr;
}

static size_t _uni_hal_io_buf_available(const uni_hal_io_buffer_t *buf) {
    return xStreamBufferBytesAvailable(buf->handle);
}

static bool _uni_hal_io_buf_reset(uni_hal_io_buffer_t *buf) {
    return xStreamBufferReset(buf->handle) == pdPASS;
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

static size_t _uni_hal_io_buf_write_isr(uni_hal_io_buffer_t *buf, const uint8_t *data, size_t data_len, BaseType_t *woken) {
    return xStreamBufferSendFromISR(buf->handle, data, data_len, woken);
}

static size_t _uni_hal_io_buf_read_isr(uni_hal_io_buffer_t *buf, uint8_t *data, size_t data_len, BaseType_t *woken) {
    return xStreamBufferReceiveFromISR(buf->handle, data, data_len, woken);
}

#else

// Without an RTOS the main loop and the interrupt handlers share the ring buffers, so every
// access runs with the interrupts masked. The sections are a few instructions long.

static bool _uni_hal_io_buf_create(uni_hal_io_buffer_t *buf) {
    buf->handle = nullptr;
    if (uni_common_ringbuffer_init(&buf->rb, (uint8_t *)buf->array, 1U, buf->size)) {
        buf->handle = &buf->rb;
    }
    return buf->handle != nullptr;
}

static size_t _uni_hal_io_buf_available(const uni_hal_io_buffer_t *buf) {
    uint32_t const primask = uni_hal_core_irq_pause();
    size_t const result = uni_common_ringbuffer_length(buf->handle);
    uni_hal_core_irq_resume(primask);
    return result;
}

static bool _uni_hal_io_buf_reset(uni_hal_io_buffer_t *buf) {
    uint32_t const primask = uni_hal_core_irq_pause();
    bool const result = uni_common_ringbuffer_clear(buf->handle);
    uni_hal_core_irq_resume(primask);
    return result;
}

static size_t _uni_hal_io_buf_read(uni_hal_io_buffer_t *buf, uint8_t *data, size_t data_len, uint32_t wait_ms) {
    // nothing to sleep on: the callers poll until their timeout
    (void)wait_ms;

    uint32_t const primask = uni_hal_core_irq_pause();
    size_t const available = uni_common_ringbuffer_length(buf->handle);
    size_t const result = uni_common_ringbuffer_pop(buf->handle, data, (available < data_len) ? available : data_len);
    uni_hal_core_irq_resume(primask);
    return result;
}

static size_t _uni_hal_io_buf_write(uni_hal_io_buffer_t *buf, const uint8_t *data, size_t data_len) {
    uint32_t const primask = uni_hal_core_irq_pause();
    // The ring buffer keeps one element free to tell full from empty, and a push into a full
    // buffer overwrites the oldest data. Only what fits is pushed, so that new data is refused
    // instead, as a stream buffer does.
    size_t const capacity = buf->handle->size_total / buf->handle->size_object - 1U;
    size_t const used = uni_common_ringbuffer_length(buf->handle);
    size_t const space = (capacity > used) ? (capacity - used) : 0U;
    size_t const result = uni_common_ringbuffer_push(buf->handle, data, (space < data_len) ? space : data_len);
    uni_hal_core_irq_resume(primask);
    return result;
}

static size_t _uni_hal_io_buf_write_isr(uni_hal_io_buffer_t *buf, const uint8_t *data, size_t data_len, BaseType_t *woken) {
    (void)woken;
    return _uni_hal_io_buf_write(buf, data, data_len);
}

static size_t _uni_hal_io_buf_read_isr(uni_hal_io_buffer_t *buf, uint8_t *data, size_t data_len, BaseType_t *woken) {
    (void)woken;
    return _uni_hal_io_buf_read(buf, data, data_len, 0U);
}

#endif


//
// Functions/Init
//

bool uni_hal_io_init(uni_hal_io_context_t *ctx) {
    bool result = false;

    if (ctx != nullptr) {
        (void) memset(&ctx->handlers, 0, sizeof(uni_hal_io_handlers_t));
        (void) memset(&ctx->stats, 0, sizeof(uni_hal_io_stats_t));

        // reset sync progress
        ctx->buf_rx.sync_idx = 0U;
        ctx->buf_tx.sync_idx = 0U;

        bool const rx_ok = _uni_hal_io_buf_create(&ctx->buf_rx);
        bool const tx_ok = _uni_hal_io_buf_create(&ctx->buf_tx);
        result = rx_ok && tx_ok;
    }

    return result;
}


//
// Functions/Buffer
//

size_t uni_hal_io_buffer_push_isr(uni_hal_io_buffer_t *buf, const uint8_t *data, size_t data_len, BaseType_t *woken) {
    size_t result = 0U;
    if (buf != nullptr && buf->handle != nullptr && data != nullptr) {
        result = _uni_hal_io_buf_write_isr(buf, data, data_len, woken);
    }
    return result;
}


size_t uni_hal_io_buffer_pop_isr(uni_hal_io_buffer_t *buf, uint8_t *data, size_t data_len, BaseType_t *woken) {
    size_t result = 0U;
    if (buf != nullptr && buf->handle != nullptr && data != nullptr) {
        result = _uni_hal_io_buf_read_isr(buf, data, data_len, woken);
    }
    return result;
}


bool uni_hal_io_buffer_is_empty(const uni_hal_io_buffer_t *buf) {
    return buf == nullptr || buf->handle == nullptr || _uni_hal_io_buf_available(buf) == 0U;
}


//
// Functions/Receive
//

size_t uni_hal_io_receive_available(const uni_hal_io_context_t *ctx) {
    size_t result = 0;
    if (ctx != nullptr && ctx->buf_rx.handle != nullptr) {
        result = _uni_hal_io_buf_available(&ctx->buf_rx);
    }
    return result;
}


bool uni_hal_io_receive_clear(uni_hal_io_context_t *ctx) {
    bool result = false;
    if (ctx != nullptr && ctx->buf_rx.handle != nullptr) {
        // reset sync progress and buffer
        ctx->buf_rx.sync_idx = 0U;
        result = _uni_hal_io_buf_reset(&ctx->buf_rx);
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

    return received;
}


bool uni_hal_io_receive_sync(uni_hal_io_context_t *ctx, const uint8_t *data, size_t data_len, uint32_t timeout) {
    // Validate inputs
    if (ctx == nullptr || ctx->buf_rx.handle == nullptr || data == nullptr || data_len == 0U) {
        return false;
    }

    const size_t pat_len = data_len;
    size_t match_idx = ctx->buf_rx.sync_idx; // resume progress across calls
    uint32_t const t_start = uni_hal_systick_get_ms();

    for (;;) {
        uint32_t const elapsed = uni_hal_systick_get_ms() - t_start;
        uint32_t const wait_ms = (elapsed < timeout) ? (timeout - elapsed) : 0U;

        uint8_t ch = 0U;
        size_t const got = _uni_hal_io_buf_read(&ctx->buf_rx, &ch, 1U, wait_ms);

        if (got == 0U) {
            // No new data. Give up once the timeout is over; until then keep looking, which is
            // only reached where the read above cannot sleep.
            if ((uni_hal_systick_get_ms() - t_start) >= timeout) {
                ctx->buf_rx.sync_idx = match_idx;
                return false;
            }
            continue;
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

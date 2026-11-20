#pragma once

#if defined(__cplusplus)
extern "C" {
#endif


//
// Include
//

// stdlib
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Uni.HAL OS layer
#include "os/uni_hal_os.h"

// FreeRTOS
#include <stream_buffer.h>



//
//Typedefs
//

typedef void (*uni_hal_io_tx_start_fn)(void *ctx_io, void *ctx_fn);

typedef void (*uni_hal_io_tx_trigger_fn)(void *ctx_io, void *ctx_fn);

typedef void (*uni_hal_io_tx_end_fn)(void *ctx_io, void *ctx_fn);



//
// Structs
//

typedef struct {
    /**
     * IO redirect context
     * @note must be a type of ::uni_hal_io_tunnel_context_t
     */
    void *tunnel_ctx;

    /**
     * TX start handler
     * @note does not called by IO, may be called by implementation
     */
    uni_hal_io_tx_start_fn tx_start;

    /**
     * TX start context
     * @note it will be passed as 2nd argument
     */
    void *tx_start_ctx;

    /**
     * TX trigger handler. Runs in the caller's task or IRQ context after bytes are queued.
     */
    uni_hal_io_tx_trigger_fn tx_trigger;

    /**
     * TX trigger context
     * @note it will be passed as 2nd argument
     */
    void *tx_trigger_ctx;

    /**
     * TX end handler
     * @note does not called by IO, may be called by implementation
     */
    uni_hal_io_tx_end_fn tx_end;

    /**
     * TX end context
     */
    void *tx_end_ctx;

} uni_hal_io_handlers_t;

typedef struct {
    /**
     * Buffer handle
     */
    StreamBufferHandle_t handle;

#if configSUPPORT_STATIC_ALLOCATION
    /**
     * Buffer control block
     */
    StaticStreamBuffer_t cb;
#endif

    /**
     * Ring size in bytes, including the one slot reserved to distinguish full from empty.
     * Dynamic-only FreeRTOS requires at least 2 bytes for one usable slot.
     */
    uint32_t size;

    /**
     * Caller-provided storage. Unused when FreeRTOS only supports dynamic allocation.
     */
    void* array;

    /**
     * Ongoing receive_sync match progress (bytes of the pattern already matched)
     * A full match is pending if its final read returned after the deadline.
     */
    size_t sync_idx;
} uni_hal_io_buffer_t;


typedef struct {
    /**
     * Number of received bytes
     */
    uint32_t rx_received;

    /**
     * Number of overrun bytes
     */
    uint32_t rx_overrun;

    /**
     * Number of transmitted bytes accepted by the transport
     */
    uint32_t tx_transmited;

    /** Bytes dequeued for transmission but discarded by the transport (e.g. RTT up-buffer full). */
    uint32_t tx_dropped;

} uni_hal_io_stats_t;


typedef struct {
    /**
     * IO RX buffer
     */
    uni_hal_io_buffer_t buf_rx;

    /**
    * IO TX buffer
    */
    uni_hal_io_buffer_t buf_tx;

    /**
     * IO handlers
     */
    uni_hal_io_handlers_t handlers;

    /**
     * IO stats
     */
    uni_hal_io_stats_t stats;

    // Identity of the marker associated with buf_rx.sync_idx. The driver only
    // reads the marker supplied to the current receive_sync call.
    uintptr_t sync_pattern_addr;
    size_t sync_pattern_len;
} uni_hal_io_context_t;


//
// Defines
//

#if defined(__cplusplus)
// Name every aggregate member for C++ builds with -Wmissing-field-initializers.
#if configSUPPORT_STATIC_ALLOCATION
#define UNI_HAL_IO_BUFFER_INITIALIZER(count, storage) \
    { .handle = nullptr, .cb = {}, .size = (count), .array = (storage), .sync_idx = 0U }
#else
#define UNI_HAL_IO_BUFFER_INITIALIZER(count, storage) \
    { .handle = nullptr, .size = (count), .array = (storage), .sync_idx = 0U }
#endif
#define UNI_HAL_IO_CONTEXT_INITIALIZER(rx_size, rx_array, tx_size, tx_array) \
    { .buf_rx = UNI_HAL_IO_BUFFER_INITIALIZER(rx_size, rx_array),            \
      .buf_tx = UNI_HAL_IO_BUFFER_INITIALIZER(tx_size, tx_array),            \
      .handlers = {}, .stats = {}, .sync_pattern_addr = 0U, .sync_pattern_len = 0U }
#else
#define UNI_HAL_IO_CONTEXT_INITIALIZER(rx_size, rx_array, tx_size, tx_array) \
    { .buf_rx = { .size = (rx_size), .array = (rx_array) },                  \
      .buf_tx = { .size = (tx_size), .array = (tx_array) } }
#endif

#if configSUPPORT_STATIC_ALLOCATION
#define UNI_HAL_IO_DEFINITION(name, count_rx, count_tx)                        \
    uint8_t name##_rx_buf[(count_rx) + 1] = {0};                               \
    uint8_t name##_tx_buf[(count_tx) + 1] = {0};                               \
    uni_hal_io_context_t name##_ctx = UNI_HAL_IO_CONTEXT_INITIALIZER(          \
        count_rx, name##_rx_buf, count_tx, name##_tx_buf)
#else
#define UNI_HAL_IO_DEFINITION(name, count_rx, count_tx)                        \
    uni_hal_io_context_t name##_ctx = UNI_HAL_IO_CONTEXT_INITIALIZER(          \
        count_rx, NULL, count_tx, NULL)
#endif

#define UNI_HAL_IO_DECLARATION(name)                                                                                     \
    extern uni_hal_io_context_t name##_ctx


//
// Functions/Init
//

/**
 * Create both buffers or reinitialise them with the configured sizes.
 * Use UNI_HAL_IO_DEFINITION or zero-initialise the context before setting its buffer
 * configuration. Reinitialisation discards queued data, handlers and statistics.
 * If either buffer cannot be created, both handles are cleared.
 * Call init/deinit outside interrupts, after stopping all users (including blocked tasks).
 * Keep the context at a fixed address while its buffers are active.
 */
bool uni_hal_io_init(uni_hal_io_context_t *ctx);

/**
 * Release the context's buffers and clear its runtime state. In a dynamic-only
 * FreeRTOS build, call this before dropping the context to return both buffers to the heap.
 * Buffer configuration is retained for the next init. Repeated deinit is safe.
 */
void uni_hal_io_deinit(uni_hal_io_context_t *ctx);


//
// Functions/Buffer
//
// For the drivers that fill and drain the buffers from their interrupt handlers.
//

/**
 * Store bytes in a buffer from an interrupt handler
 * @param buf IO buffer
 * @param data bytes to store
 * @param data_len number of bytes
 * @param woken set to pdTRUE when a task of higher priority became ready
 * @return number of bytes stored, less than data_len when the buffer is full
 */
size_t uni_hal_io_buffer_push_isr(uni_hal_io_buffer_t *buf, const uint8_t *data, size_t data_len, BaseType_t *woken);

/**
 * Take bytes out of a buffer from an interrupt handler
 * @param buf IO buffer
 * @param data receives the bytes
 * @param data_len number of bytes wanted
 * @param woken set to pdTRUE when a task of higher priority became ready
 * @return number of bytes taken, 0 when the buffer is empty
 */
size_t uni_hal_io_buffer_pop_isr(uni_hal_io_buffer_t *buf, uint8_t *data, size_t data_len, BaseType_t *woken);

/**
 * Check whether a buffer holds no data
 * @param buf IO buffer
 * @return true when empty
 */
bool uni_hal_io_buffer_is_empty(const uni_hal_io_buffer_t *buf);


//
// Functions/Receive
//

/**
 * Returns the amount of available bytes for RX
 * @param ctx  pointer to the interface context
 * @return number of available bytes
 */
size_t uni_hal_io_receive_available(const uni_hal_io_context_t *ctx);


/**
 * Discard queued RX bytes and partial synchronization from a task or a FreeRTOS-callable
 * MCU interrupt. Serialize with active RX readers. If a task is blocked on the stream,
 * return false and preserve its state.
 * @param ctx pointer to the interface context
 * @return true on success
 */
bool uni_hal_io_receive_clear(uni_hal_io_context_t *ctx);


/**
 * Receive data. From a FreeRTOS-callable MCU interrupt, read only the bytes
 * already available, regardless of timeout. Serialize readers of the same RX buffer.
 * @param ctx pointer to the interface context
 * @param data pointer to the data array (must be allocated by the caller)
 * @param data_len number of bytes to receive
 * @param timeout finite receive budget in milliseconds, including UINT32_MAX
 * @return number of bytes which were received
 */
size_t uni_hal_io_receive_data(uni_hal_io_context_t *ctx, uint8_t *data, uint32_t data_len, uint32_t timeout);


/**
 * Consume RX bytes through the first complete marker, leaving later bytes queued.
 * @param timeout finite search budget in milliseconds, including UINT32_MAX. Zero polls only the bytes
 *        already queued on entry. A FreeRTOS-callable MCU interrupt always
 *        uses this nonblocking polling mode.
 * @return true when the marker was found
 * @note A partial match resumes on the next call with the same marker address and
 *       length. Keep its bytes unchanged between retries. Call uni_hal_io_receive_sync_reset
 *       before reusing that address for another marker of the same length.
 *       Serialize RX readers. Consuming bytes with uni_hal_io_receive_data or
 *       uni_hal_io_buffer_pop_isr discards saved progress. Reset it explicitly
 *       after direct reads through the FreeRTOS stream handle.
 * @note A final byte read exactly at the deadline completes the search. If the
 *       blocking read returns later, this call returns false and the next search
 *       for the same marker reports the match once.
 */
bool uni_hal_io_receive_sync(uni_hal_io_context_t *ctx, const uint8_t *data, size_t data_len, uint32_t timeout);

/** Discard saved marker progress and preserve the queued RX data. */
void uni_hal_io_receive_sync_reset(uni_hal_io_context_t *ctx);



//
// Functions/Transmit
//

/**
 * Send data
 * @param ctx pointer to interface context
 * @param data pointer to data array
 * @param data_len length of data array in bytes
 * @return number of transmited bytes
 */
size_t uni_hal_io_transmit_data(uni_hal_io_context_t *ctx, const uint8_t *data, uint32_t data_len);



#if defined(__cplusplus)
}
#endif

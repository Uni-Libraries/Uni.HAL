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

// FreeRTOS
#include <FreeRTOS.h>
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
     * TX trigger handler
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

    /**
     * Buffer control block
     */
    StaticStreamBuffer_t cb;

    /**
     * Buffer size
     */
    uint32_t size;

    /**
     * Buffer array
     */
    void* array;

    /**
     * Ongoing receive_sync match progress (bytes of the pattern already matched)
     * This allows continuing sync across calls when a timeout occurs mid-pattern.
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
     * Number of transmited bytes
     */
    uint32_t tx_transmited;

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
} uni_hal_io_context_t;


//
// Defines
//

#define UNI_HAL_IO_DEFINITION(name, count_rx, count_tx)                                                                  \
    uint8_t name##_rx_buf[count_rx+1] = {0};                                                                            \
    uint8_t name##_tx_buf[count_tx+1] = {0};                                                                            \
    uni_hal_io_context_t name##_ctx = {                                                                                  \
        .buf_rx = {                                                                                                     \
            .size  = count_rx      ,                                                                                    \
            .array = &name##_rx_buf,                                                                                    \
        },                                                                                                              \
       .buf_tx = {                                                                                                      \
            .size  = count_tx      ,                                                                                    \
            .array = &name##_tx_buf,                                                                                    \
       },                                                                                                               \
    }                                                                                                                   \

#define UNI_HAL_IO_DECLARATION(name)                                                                                     \
    extern uni_hal_io_context_t name##_ctx


//
// Functions/Init
//

bool uni_hal_io_init(uni_hal_io_context_t *ctx);


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
 * Clear RX buffer
 * @param ctx pointer to the interface context
 * @return true on success
 */
bool uni_hal_io_receive_clear(uni_hal_io_context_t *ctx);


/**
 * Receive data
 * @param ctx pointer to the interface context
 * @param data pointer to the data array (must be allocated by the caller)
 * @param data_len number of bytes to receive
 * @param timeout timeout in msecs
 * @return number of bytes which were received
 */
size_t uni_hal_io_receive_data(uni_hal_io_context_t *ctx, uint8_t *data, uint32_t data_len, uint32_t timeout);


/**
 * Receive line
 * @param ctx pointer to interface context
 * @param data pointer to data array
 * @param data_len length of data array in bytes
 * @param timeout receive timeout in msecs
 * @return number of received bytes
 */
size_t uni_hal_io_receive_line(uni_hal_io_context_t *ctx, uint8_t *data, uint32_t data_len, uint32_t timeout);

/**
 * Skip input data until specified data will be found
 * @param ctx pointer to the interface context
 * @param data pointer to the data array
 * @param data_len length of the data array in bytes
 * @param timeout receive timoeut in msecs
 * @return true in case of successful sync
 */
bool uni_hal_io_receive_sync(uni_hal_io_context_t* ctx, const uint8_t* data, size_t data_len, uint32_t timeout);



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

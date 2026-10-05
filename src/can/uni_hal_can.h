#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// FreeRTOS
#if defined(UNI_HAL_CAN_USE_FREERTOS)
#include <FreeRTOS.h>
#include <queue.h>
#endif

// Uni.Common
#include "uni_common.h"

// Uni.HAL
#include "core/uni_hal_core.h"
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc_enum.h"



//
// Defines
//

#define UNI_HAL_CAN_QUEUE_SIZE (32U)



//
// Typedefs
//

/**
 * CAN message
 */
typedef struct {
    uint32_t id;

    uint8_t dlc;

    uint8_t data[8];

    /**
     * The identifier is a standard 11-bit one. false, the default, means a 29-bit extended
     * identifier, which is what the driver has always sent.
     */
    bool standard_id;

    /**
     * Remote frame: a request for the data of this identifier. It carries no data bytes; dlc is
     * the length that is asked for.
     */
    bool remote;
} uni_hal_can_msg_t;

/**
 * How the node takes part in bus traffic
 */
typedef enum {
    /** send and receive */
    UNI_HAL_CAN_MODE_NORMAL = 0,

    /** listen only: frames are received, but the node sends nothing, not even an acknowledge */
    UNI_HAL_CAN_MODE_SILENT,

    /**
     * Every frame that is sent is also received by the node itself. The frames still go out on
     * the TX pin; the RX pin is ignored.
     */
    UNI_HAL_CAN_MODE_LOOPBACK,

    /**
     * Loop back with nothing reaching the pins: for a self-test of the software without a bus
     * or a transceiver
     */
    UNI_HAL_CAN_MODE_LOOPBACK_SILENT,
} uni_hal_can_mode_e;


/**
 * CAN config
 */
typedef struct
{
    /**
     * CAN instance
     */
    uni_hal_core_periph_e instance;

    uni_hal_gpio_pin_context_t* pin_rx;

    uni_hal_gpio_pin_context_t* pin_tx;

    /**
     * Kernel clock of the peripheral, STM32H7 only: UNI_HAL_RCC_CLKSRC_HSE, _PLL1Q or _PLL2Q.
     * Both FDCAN instances share it. UNI_HAL_RCC_CLKSRC_UNKNOWN (0) keeps the source that is
     * selected, which is HSE after reset. The bxCAN of the STM32L4 always runs from APB1.
     */
    uni_hal_rcc_clksrc_e clock_source;

    /**
     * Operating mode; the zero value is normal operation
     */
    uni_hal_can_mode_e mode;

    /**
     * Bit rate in bit/s. The bit timing is computed from the CAN clock with a sample point
     * near 87.5 %, and the initialisation fails when the clock cannot give this rate exactly.
     * 0 keeps the former fixed timing on the STM32L4: prescaler 10, 10 time quanta per bit.
     * The STM32H7 driver has no such default and needs a bit rate.
     */
    uint32_t bitrate;

    /**
     * Retransmit a frame that lost arbitration or was hit by an error (clears NART)
     */
    bool auto_retransmission;

    /**
     * Leave the bus-off state automatically after 128 x 11 recessive bits (sets ABOM)
     */
    bool auto_bus_off;

    /**
     * Wake up from sleep mode when a frame is detected (sets AWUM)
     */
    bool auto_wake_up;

    /**
     * Send the queued frames in the order they were requested instead of by identifier (sets TXFP)
     */
    bool tx_fifo_priority;

#if !defined(UNI_HAL_CAN_USE_FREERTOS)
    uni_common_ringbuffer_context_t *buffer_rx;
#endif

} uni_hal_can_config_t;


/**
 * Called from the CAN transmit interrupt when a frame has left its TX mailbox
 * @param cookie value given to uni_hal_can_set_tx_callback()
 * @param mailbox mailbox the frame was in, 0..2
 * @param success true when the frame was transmitted and acknowledged; false when it was lost
 *                (bus error or lost arbitration with automatic retransmission off) or aborted
 * @return true when the callback made a task of higher priority ready
 * @note runs in interrupt context; uni_hal_can_transmit_nowait() may be called from it
 */
typedef bool (*uni_hal_can_tx_callback_t)(void *cookie, uint32_t mailbox, bool success);


/**
 * CAN bus errors, combined as bits in uni_hal_can_status_t::errors
 */
typedef enum {
    UNI_HAL_CAN_ERROR_NONE       = 0,
    /** an error counter reached the warning limit (96) */
    UNI_HAL_CAN_ERROR_WARNING    = 1 << 0,
    /** the node is error passive (an error counter above 127) */
    UNI_HAL_CAN_ERROR_PASSIVE    = 1 << 1,
    /** the node went bus-off */
    UNI_HAL_CAN_ERROR_BUS_OFF    = 1 << 2,
    /** a hardware receive FIFO overflowed and a frame was lost */
    UNI_HAL_CAN_ERROR_RX_OVERRUN = 1 << 3,
    /** a received frame was dropped because the receive queue of the driver was full */
    UNI_HAL_CAN_ERROR_RX_DROPPED = 1 << 4,
} uni_hal_can_error_e;


/**
 * Called from a CAN interrupt when the driver detects a bus error or loses a frame
 * @param cookie value given to uni_hal_can_set_error_callback()
 * @param errors what happened, bits of uni_hal_can_error_e. The same bits are also collected in
 *               uni_hal_can_status_t::errors.
 * @return true when the callback made a task of higher priority ready
 * @note runs in interrupt context
 */
typedef bool (*uni_hal_can_error_callback_t)(void *cookie, uint32_t errors);


/**
 * CAN status
 */
typedef struct {

    uint32_t count_rx;

    uint32_t count_tx;

    uint32_t count_err;

    /**
     * Received frames that were dropped because the receive queue was full
     */
    uint32_t count_rx_dropped;

    /**
     * Bus errors seen since the initialisation, bits of uni_hal_can_error_e.
     * The application may clear the field after reading it.
     */
    uint32_t errors;

    bool inited;

    /**
     * Transmit completion callback, see uni_hal_can_set_tx_callback()
     */
    uni_hal_can_tx_callback_t tx_callback;

    /**
     * Value passed to the transmit completion callback
     */
    void *tx_callback_cookie;

    /**
     * Outcome of the last frame of each TX mailbox, kept by the transmit interrupt
     */
    volatile bool tx_ok[3];

    /**
     * Error callback, see uni_hal_can_set_error_callback()
     */
    uni_hal_can_error_callback_t error_callback;

    /**
     * Value passed to the error callback
     */
    void *error_callback_cookie;

#if defined(UNI_HAL_CAN_USE_FREERTOS)
    QueueHandle_t queue_rx;
#endif

} uni_hal_can_status_t;

/**
 * CAN context
 */
typedef struct
{
    uni_hal_can_config_t config;
    uni_hal_can_status_t status;
} uni_hal_can_context_t;



/**
 * CAN bit timing, in time quanta
 */
typedef struct {
    /** clock divider, 1..1024 */
    uint32_t prescaler;
    /** bit segment 1 (propagation + phase 1), 1..16 */
    uint32_t bs1;
    /** bit segment 2 (phase 2), 1..8 */
    uint32_t bs2;
    /** synchronisation jump width, 1..4 */
    uint32_t sjw;
} uni_hal_can_timing_t;



//
// Functions
//

/**
 * Compute a bit timing for the bxCAN peripheral
 * @param clock_hz frequency of the CAN clock (APB1)
 * @param bitrate bit rate in bit/s
 * @param timing receives the timing
 * @return false when no setting gives exactly this bit rate
 */
bool uni_hal_can_timing_calc(uint32_t clock_hz, uint32_t bitrate, uni_hal_can_timing_t *timing);

/**
 * Check that CAN RQ queue contains at least one incoming message
 * @param ctx CAN context
 * @return number of incoming messages
 */
uint32_t uni_hal_can_is_available(const uni_hal_can_context_t *ctx);

bool uni_hal_can_init(uni_hal_can_context_t *ctx);

/**
 * Checks that CAN module was properly inited
 * @param ctx pointer to the CAN context
 * @return true in case of inited CAN
 */
bool uni_hal_can_is_inited(const uni_hal_can_context_t*ctx);

bool uni_hal_can_start(uni_hal_can_context_t *ctx);

bool uni_hal_can_stop(uni_hal_can_context_t *ctx);

bool uni_hal_can_set_filter(uni_hal_can_context_t *ctx, uint32_t fifo_num, uint32_t slot_idx, uint32_t filter_id,
                           uint32_t filter_mask);

bool uni_hal_can_receive(uni_hal_can_context_t *ctx, uni_hal_can_msg_t *msg, size_t timeout_ms);

/**
 * Send a frame and wait until it has left its mailbox
 * @param ctx CAN context
 * @param msg frame to send
 * @return true when the frame was transmitted and acknowledged
 */
bool uni_hal_can_transmit(uni_hal_can_context_t *ctx, uni_hal_can_msg_t *msg);

/**
 * Queue a frame for transmission and return at once
 * @param ctx CAN context
 * @param msg frame to send
 * @return false when no TX mailbox is free, see uni_hal_can_transmit_free()
 */
bool uni_hal_can_transmit_nowait(uni_hal_can_context_t *ctx, const uni_hal_can_msg_t *msg);

/**
 * Get the number of free TX mailboxes
 * @param ctx CAN context
 * @return 0..3
 */
uint32_t uni_hal_can_transmit_free(const uni_hal_can_context_t *ctx);

/**
 * Register a function that is called whenever a frame has left its TX mailbox, successfully or
 * not. This is how a caller of uni_hal_can_transmit_nowait() learns what became of its frames
 * and when a mailbox is free again.
 * Register it before the first frame is sent: frames that completed earlier are not reported,
 * and a blocking uni_hal_can_transmit() that is under way in another task loses its result.
 * @param ctx CAN context, initialised
 * @param callback function to call; nullptr removes the callback
 * @param cookie value passed back to the callback
 * @return true on success
 */
bool uni_hal_can_set_tx_callback(uni_hal_can_context_t *ctx, uni_hal_can_tx_callback_t callback, void *cookie);

/**
 * Register a function that is called when the error state of the node changes (error warning,
 * error passive, bus-off) or a received frame is lost. Without it the application has to poll
 * uni_hal_can_status_t::errors.
 * @param ctx CAN context, initialised
 * @param callback function to call; nullptr removes the callback
 * @param cookie value passed back to the callback
 * @return true on success
 */
bool uni_hal_can_set_error_callback(uni_hal_can_context_t *ctx, uni_hal_can_error_callback_t callback, void *cookie);

/**
 * Drop every frame that is still waiting in a TX mailbox, e.g. after the bus went away
 * @param ctx CAN context
 * @return true on success
 */
bool uni_hal_can_transmit_abort(uni_hal_can_context_t *ctx);

#if defined(__cplusplus)
}
#endif

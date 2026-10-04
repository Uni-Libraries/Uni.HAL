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
#include <FreeRTOS.h>
#include <queue.h>

// Uni.Common
#include "uni_common.h"

// Uni.HAL
#include "core/uni_hal_core.h"
#include "gpio/uni_hal_gpio.h"



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
} uni_hal_can_msg_t;

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
     * Bit rate in bit/s. The bit timing is computed from the CAN clock with a sample point
     * near 87.5 %, and the initialisation fails when the clock cannot give this rate exactly.
     * 0 keeps the former fixed timing: prescaler 10, 10 time quanta per bit.
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
} uni_hal_can_error_e;


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

bool uni_hal_can_transmit(uni_hal_can_context_t *ctx, uni_hal_can_msg_t *msg);

#if defined(__cplusplus)
}
#endif

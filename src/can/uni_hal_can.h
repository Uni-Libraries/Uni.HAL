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

#if !defined(UNI_HAL_CAN_USE_FREERTOS)
    uni_common_ringbuffer_context_t *buffer_rx;
#endif

} uni_hal_can_config_t;


/**
 * CAN status
 */
typedef struct {

    uint32_t count_rx;

    uint32_t count_tx;

    uint32_t count_err;

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



//
// Functions
//

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

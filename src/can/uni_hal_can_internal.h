#pragma once

#include <stddef.h>

#include "can/uni_hal_can.h"

/** Search the nominal bit timing supported by the STM32H743/H753 FDCAN NBTP register. */
bool uni_hal_can_timing_calc_fdcan_nominal(uint32_t clock_hz, uint32_t bitrate, uni_hal_can_timing_t *timing);

static inline QueueHandle_t uni_hal_can_init_rx_queue(QueueHandle_t *queue) {
    if (*queue == NULL) {
        *queue = xQueueCreate(UNI_HAL_CAN_QUEUE_SIZE, sizeof(uni_hal_can_msg_t));
    } else if (xQueueReset(*queue) != pdPASS) {
        return NULL;
    }
    return *queue;
}

#pragma once

#include "can/uni_hal_can.h"

static inline QueueHandle_t uni_hal_can_init_rx_queue(QueueHandle_t *queue) {
    if (*queue == nullptr) {
        *queue = xQueueCreate(UNI_HAL_CAN_QUEUE_SIZE, sizeof(uni_hal_can_msg_t));
    } else if (xQueueReset(*queue) != pdPASS) {
        return nullptr;
    }
    return *queue;
}

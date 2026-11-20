#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <FreeRTOS.h>

// The driver includes the device header for the CMSIS priority width and grouping.
static inline bool uni_hal_os_irq_priority_valid(uint32_t priority) {
    uint32_t const minimum = configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS);
    return priority >= minimum && priority < (1U << __NVIC_PRIO_BITS) &&
           NVIC_GetPriorityGrouping() <= 7U - __NVIC_PRIO_BITS;
}

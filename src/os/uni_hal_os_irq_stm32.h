#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <FreeRTOS.h>

// The driver includes the device header for the CMSIS priority width and grouping.
#define UNI_HAL_OS_IRQ_MIN_PRIORITY (configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS))

static inline uint32_t uni_hal_os_irq_priority_for_kernel(uint32_t preferred) {
    return preferred < UNI_HAL_OS_IRQ_MIN_PRIORITY ? UNI_HAL_OS_IRQ_MIN_PRIORITY : preferred;
}

static inline bool uni_hal_os_irq_priority_valid(uint32_t priority) {
    return priority >= UNI_HAL_OS_IRQ_MIN_PRIORITY && priority < (1U << __NVIC_PRIO_BITS) &&
           NVIC_GetPriorityGrouping() <= 7U - __NVIC_PRIO_BITS;
}

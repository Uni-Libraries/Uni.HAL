#pragma once
#include <stdbool.h>
#include <stdint.h>
#define UNI_HAL_CORE_ZYNQ7000_IRQ_PRIORITY_DEFAULT 20U
typedef void (*Xil_InterruptHandler)(void *);
bool uni_hal_core_zynq7000_irq_connect(uint32_t id, Xil_InterruptHandler handler, void *ctx,
                                       uint32_t priority, bool edge);

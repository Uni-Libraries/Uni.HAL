#pragma once
#include <stdbool.h>
#include <stdint.h>
#define UNI_HAL_CORE_ZYNQ7000_IRQ_PRIORITY_DEFAULT 20U
// The host model uses the logical priority levels of the Cortex-A9 FreeRTOS port.
#ifndef configMAX_API_CALL_INTERRUPT_PRIORITY
#define configMAX_API_CALL_INTERRUPT_PRIORITY 18U
#endif
#define portLOWEST_USABLE_INTERRUPT_PRIORITY 30U
typedef void (*Xil_InterruptHandler)(void *);
bool uni_hal_core_zynq7000_irq_connect(uint32_t id, Xil_InterruptHandler handler, void *ctx,
                                       uint32_t priority, bool edge);
bool uni_hal_core_zynq7000_global_timer_irq_available(void);
typedef struct { unsigned unused; } XScuGic;
extern XScuGic g_uni_hal_core_zynq7000_gic;
void XScuGic_Disconnect(XScuGic *gic, uint32_t id);

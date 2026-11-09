#pragma once

//
// Zynq-7000 (ps7_cortexa9_0): GIC, vector table, FreeRTOS tick.
//
// Start-up order in main():
//   uni_hal_core_zynq7000_init();   // VBAR, global timer, GIC
//   ... create tasks, drivers ...
//   vTaskStartScheduler();          // calls vConfigureTickInterrupt()
//
// Interrupt handlers are registered in the BSP table XScuGic_ConfigTable[0].HandlerTable,
// either through uni_hal_core_zynq7000_irq_connect() or through the static
// XScuGic_RegisterHandler() of the BSP (used, e.g., by the FreeRTOS+TCP Zynq driver).
//

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stdint.h>

// Xilinx BSP
#include <xil_exception.h>
#include <xscugic.h>



//
// Defines
//

/** GIC priority (0..31, lower is more urgent) used by default for the Uni.HAL drivers.
 *  Must be numerically >= configMAX_API_CALL_INTERRUPT_PRIORITY (18) to call ...FromISR. */
#define UNI_HAL_CORE_ZYNQ7000_IRQ_PRIORITY_DEFAULT (20U)



//
// Globals
//

/** The single GIC instance shared by all drivers */
extern XScuGic g_uni_hal_core_zynq7000_gic;



//
// Functions
//

/**
 * Installs the vector table (IRQ/SVC to FreeRTOS, other exceptions to the BSP), starts the
 * global timer (XTime) and initializes the GIC. Call first in main().
 */
void uni_hal_core_zynq7000_init(void);

/**
 * Connects a handler to a GIC interrupt (SPI/PPI id) and enables it.
 * @param irq_id GIC interrupt id, e.g. XPS_UART0_INT_ID
 * @param priority GIC priority 0..31
 * @param rising_edge true for edge-triggered SPIs, false for level-sensitive
 */
bool uni_hal_core_zynq7000_irq_connect(uint32_t irq_id, Xil_InterruptHandler handler, void *ctx, uint32_t priority,
                                       bool rising_edge);

#if defined(__cplusplus)
}
#endif

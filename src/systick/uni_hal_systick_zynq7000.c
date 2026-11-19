//
// Zynq-7000: millisecond time base on the Cortex-A9 global timer (XTime, CPU_CLK/2), which runs
// independently of the FreeRTOS tick and is started by uni_hal_core_zynq7000_init().
//

//
// Includes
//

// Xilinx BSP
#include <xiltimer.h>

// FreeRTOS
#include <FreeRTOS.h>
#include <task.h>

// uni_hal
#include "systick/uni_hal_systick.h"



//
// Defines
//

/* COUNTS_PER_SECOND of the BSP expands without parentheses (CPU_HZ/2) */
#define UNI_HAL_SYSTICK_ZYNQ7000_COUNTS_PER_MS ((uint64_t)(COUNTS_PER_SECOND) / 1000U)



//
// Functions
//

bool uni_hal_systick_init(void) {
    return true;
}


uint32_t uni_hal_systick_get_ms(void) {
    XTime t;
    XTime_GetTime(&t);
    return (uint32_t)(t / UNI_HAL_SYSTICK_ZYNQ7000_COUNTS_PER_MS);
}


uint32_t uni_hal_systick_get_tick(void) {
    return uni_hal_systick_get_ms();
}


uint32_t uni_hal_systick_get_freq(void) {
    return 1000U;
}


void uni_hal_systick_delay(uint32_t val) {
    if (val == 0U) {
        val = 1U;
    }
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(val));
        return;
    }
    uint32_t start = uni_hal_systick_get_ms();
    while ((uni_hal_systick_get_ms() - start) < val) {
    }
}

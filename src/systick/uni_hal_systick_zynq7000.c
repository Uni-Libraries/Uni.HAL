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


// Convert the remaining global-timer counts to a finite scheduler wait, rounded up.
// Splitting off whole seconds keeps the product in range for long delays.
static TickType_t delay_ticks(XTime counts) {
    uint64_t const hz = (COUNTS_PER_SECOND);
    uint64_t const seconds = counts / hz;
    uint64_t const fraction = counts % hz;
    uint64_t const ticks = seconds * configTICK_RATE_HZ + (fraction * configTICK_RATE_HZ + hz - 1U) / hz;
    return ticks < portMAX_DELAY ? (TickType_t)ticks : portMAX_DELAY - 1U;
}

void uni_hal_systick_delay(uint32_t val) {
    uint64_t const ms = val != 0U ? val : 1U;
    XTime const duration = (ms * (COUNTS_PER_SECOND) + 999U) / 1000U;
    bool const scheduled = xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    XTime start;
    XTime_GetTime(&start);
    XTime elapsed = 0U;
    while (elapsed < duration) {
        if (scheduled) {
            vTaskDelay(delay_ticks(duration - elapsed));
        }
        // A tick can arrive immediately after vTaskDelay blocks the task.
        XTime now;
        XTime_GetTime(&now);
        elapsed = now - start;
    }
}

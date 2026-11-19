//
// Includes
//

// Uni.HAL
#include "systick/uni_hal_systick.h"



//
// Handlers
//

#if defined(UNI_HAL_TARGET_MCU_STM32L496) || defined(UNI_HAL_TARGET_MCU_STM32H743)

#include <FreeRTOS.h>
#include <task.h>

extern void xPortSysTickHandler(void);

extern volatile uint32_t g_uni_hal_systick_counter;

__attribute__((unused)) void SysTick_Handler(void) {
    g_uni_hal_systick_counter++;

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

#endif

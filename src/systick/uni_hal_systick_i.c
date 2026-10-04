//
// Includes
//

// Uni.HAL
#include "systick/uni_hal_systick.h"



//
// Handlers
//

#if !defined(UNI_HAL_TARGET_MCU_PC)

#if defined(UNI_HAL_USE_FREERTOS)
extern void xPortSysTickHandler(void);
extern uint32_t xTaskGetSchedulerState( void );
#define taskSCHEDULER_NOT_STARTED    ( 1U )
#endif

extern volatile uint32_t g_uni_hal_systick_counter;

__attribute__((unused)) void SysTick_Handler(void) {
    g_uni_hal_systick_counter++;

#if defined(UNI_HAL_USE_FREERTOS)
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
#endif
}

#endif

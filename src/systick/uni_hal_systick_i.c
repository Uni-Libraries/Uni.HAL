//
// Includes
//

// uni_hal
#include "systick/uni_hal_hal_systick.h"



//
// Extern
//

extern void xPortSysTickHandler(void);
extern uint32_t xTaskGetSchedulerState( void );
#define taskSCHEDULER_NOT_STARTED    ( 1U )

extern volatile uint32_t g_uni_hal_systick_counter;



//
// Handlers
//


__attribute__((unused)) void SysTick_Handler(void) {
    g_uni_hal_systick_counter++;

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

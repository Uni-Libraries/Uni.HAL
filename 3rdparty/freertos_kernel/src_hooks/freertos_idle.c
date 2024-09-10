//
// Includes
//

#include <FreeRTOS.h>



//
// Functions
//

void vApplicationGetIdleTaskMemory (StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
    static StaticTask_t Idle_TCB;
    static StackType_t Idle_Stack[configMINIMAL_STACK_SIZE];

    /* Idle task control block and stack */
    *ppxIdleTaskTCBBuffer = &Idle_TCB;
    *ppxIdleTaskStackBuffer = &Idle_Stack[0];
    *pulIdleTaskStackSize = (uint32_t) configMINIMAL_STACK_SIZE;
}

//
// Includes
//

#include <FreeRTOS.h>
#include <task.h>



//
// Functions
//

void vApplicationGetIdleTaskMemory (StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, configSTACK_DEPTH_TYPE *pulIdleTaskStackSize) {
    static StaticTask_t Idle_TCB;
    static StackType_t Idle_Stack[configMINIMAL_STACK_SIZE];

    /* Idle task control block and stack */
    *ppxIdleTaskTCBBuffer = &Idle_TCB;
    *ppxIdleTaskStackBuffer = &Idle_Stack[0];
    *pulIdleTaskStackSize = (configSTACK_DEPTH_TYPE) configMINIMAL_STACK_SIZE;
}

#include <FreeRTOS.h>
#include <task.h>

void vApplicationStackOverflowHook( TaskHandle_t pxTask,
                                    char * pcTaskName ) {
    (void)pxTask;
    (void)pcTaskName;
    volatile uint32_t c = 0;
    while (!c) {
    }
}

#include <stdio.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

void vApplicationMallocFailedHook(void) {
    fputs("Cannot allocate a minimum-size task with the default PC heap.\n", stderr);
    _Exit(EXIT_FAILURE);
}

static void run_task(void *unused) {
    (void)unused;
    // Exercise a context switch after the scheduler creates its own idle task.
    vTaskDelay(2U);
    _Exit(EXIT_SUCCESS);
}

int main(void) {
    if (xTaskCreate(run_task, "probe", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1U, NULL) != pdPASS) {
        return EXIT_FAILURE;
    }
    vTaskStartScheduler();
    return EXIT_FAILURE;
}

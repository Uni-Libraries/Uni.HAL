//
// Includes
//

// stdlib
#include <stdint.h>

// FreeRTOS
#include <FreeRTOS.h>
#include <task.h>



//
// Functions
//

void vAssertCalled(uint32_t ulLine, const char *pcFile) {
    volatile unsigned long ul = 0;

    (void)pcFile;
    (void)ulLine;

    taskENTER_CRITICAL();
    {
        while (ul == 0) {
        }
    }
    taskEXIT_CRITICAL();
}

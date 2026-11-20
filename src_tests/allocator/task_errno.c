#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#if configAPPLICATION_ALLOCATED_HEAP
alignas(portBYTE_ALIGNMENT) uint8_t ucHeap[configTOTAL_HEAP_SIZE];
#endif

static TaskHandle_t first;
static TaskHandle_t second;
static int *second_errno;

[[noreturn]] static void finish(uint32_t result) {
    uint32_t args[] = {0x20026U, result};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *parameters __asm__("r1") = args;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(parameters) :: "memory");
    for (;;) {}
}

static void second_task(void *unused) {
    (void)unused;
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // strtok keeps its cursor in libc's task state as well.
    char tokens[] = "C,D";
    if (strtok(tokens, ",") != tokens) {
        finish(1);
    }
    second_errno = &errno;
    errno = ERANGE;
    xTaskNotifyGive(first);
    vTaskSuspend(nullptr);
}

static void first_task(void *unused) {
    (void)unused;
    char tokens[] = "A,B";
    if (strtok(tokens, ",") != tokens) {
        finish(2);
    }
    errno = EDOM;
    xTaskNotifyGive(second);
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (errno != EDOM || second_errno == &errno) {
        finish(3);
    }
    vTaskDelete(second);
    if (strtok(nullptr, ",") != &tokens[2]) {
        finish(4);
    }
    finish(0);
}

int main(void) {
    if (xTaskCreate(first_task, "first", 512, nullptr, 1, &first) != pdPASS ||
        xTaskCreate(second_task, "second", 512, nullptr, 1, &second) != pdPASS) {
        finish(5);
    }
    vTaskStartScheduler();
    finish(6);
}

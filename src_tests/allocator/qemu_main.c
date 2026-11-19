#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

extern int __real_main(void);
extern int uni_hal_test_io_buffers(void);

[[noreturn]] static void exit_qemu(int result) {
    // Arm semihosting SYS_EXIT_EXTENDED reports the test's return code to CTest.
    uint32_t block[] = {0x20026U, (uint32_t)result};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = block;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

static void run_in_task(void *unused) {
    (void)unused;
    vTaskSuspendAll();
    void *block = malloc(16U);
    if (!block) {
        exit_qemu(1);
    }
    free(block);
    (void)xTaskResumeAll();
    int result = __real_main();
    if (result == 0) {
        result = uni_hal_test_io_buffers();
    }
    exit_qemu(result);
}

int __wrap_main(void) {
    int result = __real_main();
    if (result == 0 && xTaskCreate(run_in_task, "allocator", 512, nullptr, 1, nullptr) == pdPASS) {
        vTaskStartScheduler();
    }
    exit_qemu(result ? result : 1);
}

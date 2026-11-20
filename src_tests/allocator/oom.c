#include <stdint.h>
#include <stm32l4xx.h>
#include <FreeRTOS.h>
#include <task.h>

#include "io/uni_hal_io.h"

#if configAPPLICATION_ALLOCATED_HEAP
alignas(portBYTE_ALIGNMENT) uint8_t ucHeap[configTOTAL_HEAP_SIZE];
#endif

static volatile bool expecting_failure;

[[noreturn]] static void finish(unsigned code) {
    const uint32_t result[] = {0x20026U, code};
    register uint32_t operation __asm__("r0") = 0x20U;
    register const void *args __asm__("r1") = result;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

#define CHECK(condition) do { if (!(condition)) finish(__LINE__); } while (0)

#if configUSE_MALLOC_FAILED_HOOK && defined(TEST_APPLICATION_OOM_HOOK)
static unsigned hook_calls;
static size_t free_at_failure;

void vApplicationMallocFailedHook(void) {
    CHECK(expecting_failure);
    ++hook_calls;
    free_at_failure = xPortGetFreeHeapSize();
}
#elif configUSE_MALLOC_FAILED_HOOK
void UsageFault_Handler(void) {
    // The shipped Cortex-M hook traps on OOM. Distinguish its UDF from other faults.
    finish(expecting_failure && (SCB->CFSR & SCB_CFSR_UNDEFINSTR_Msk) ? 0U : __LINE__);
}
#endif

static void test_oom(void *unused) {
    (void)unused;
    size_t const initial_free = xPortGetFreeHeapSize();
    HeapStats_t stats;
    vPortGetHeapStats(&stats);
    CHECK(stats.xSizeOfLargestFreeBlockInBytes > 512U);
    void *pressure = pvPortMalloc(stats.xSizeOfLargestFreeBlockInBytes - 512U);
    CHECK(pressure != nullptr);
    size_t const free_before = xPortGetFreeHeapSize();

    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;
    expecting_failure = true;
#if configSUPPORT_STATIC_ALLOCATION
    CHECK(pvPortMalloc(free_before) == nullptr); // heap_4 also needs a block header.
#else
    // RX fits; TX requires more than the remaining heap and must roll back RX.
    UNI_HAL_IO_DEFINITION(io, 16U, 1024U);
    CHECK(!uni_hal_io_init(&io_ctx));
    CHECK(io_ctx.buf_rx.handle == nullptr && io_ctx.buf_tx.handle == nullptr);
#endif
    expecting_failure = false;
#if configUSE_MALLOC_FAILED_HOOK
#if defined(TEST_APPLICATION_OOM_HOOK)
    CHECK(hook_calls == 1U);
#if !configSUPPORT_STATIC_ALLOCATION
    CHECK(free_at_failure < free_before); // RX had been allocated when TX failed.
#endif
#else
    finish(__LINE__); // The default hook must already have trapped.
#endif
#endif
    CHECK(xPortGetFreeHeapSize() == free_before);
    vPortFree(pressure);
#if !configSUPPORT_STATIC_ALLOCATION
    CHECK(uni_hal_io_init(&io_ctx));
    uni_hal_io_deinit(&io_ctx);
#endif
    CHECK(xPortGetFreeHeapSize() == initial_free);
    finish(0);
}

int main(void) {
    CHECK(xTaskCreate(test_oom, "oom", 512U, nullptr, 1U, nullptr) == pdPASS);
    vTaskStartScheduler();
    finish(__LINE__);
}

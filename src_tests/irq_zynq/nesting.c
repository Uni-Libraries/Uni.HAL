#include "uni_hal.h"
#include <xil_exception.h>

#if configAPPLICATION_ALLOCATED_HEAP
alignas(portBYTE_ALIGNMENT) uint8_t ucHeap[configTOTAL_HEAP_SIZE];
#endif

static void finish(uint32_t code) {
    uint32_t report[] = {0x20026U, code};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = report;
    __asm__ volatile ("svc 0x123456" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

#define CHECK(expr) do { if (!(expr)) finish(__LINE__); } while (0)

static bool irq_masked(void) {
    return (mfcpsr() & XIL_EXCEPTION_IRQ) != 0U;
}

enum { test_priority = tskIDLE_PRIORITY + 1U };
static volatile bool peer_ran;

static void peer(void *unused) {
    (void)unused;
    CHECK(!irq_masked());
    taskENTER_CRITICAL();
    CHECK(irq_masked());
    taskEXIT_CRITICAL();
    CHECK(!irq_masked());
    peer_ran = true;
    vTaskDelete(nullptr);
}

static void run(void *unused) {
    (void)unused;
    CHECK(!irq_masked());
    uint32_t const saved = uni_hal_core_irq_pause();
    CHECK(irq_masked());
    uint32_t const priority_mask = portSET_INTERRUPT_MASK_FROM_ISR();
    CHECK(irq_masked());
    portCLEAR_INTERRUPT_MASK_FROM_ISR(priority_mask);
    CHECK(irq_masked());
    taskENTER_CRITICAL();
    taskENTER_CRITICAL();
    taskEXIT_CRITICAL();
    CHECK(irq_masked());
    // Creating a higher-priority peer yields inside this critical section. The
    // scheduler can resume us only after the peer has checked its own IRQ mask.
    CHECK(xTaskCreate(peer, "peer", 512U, nullptr, test_priority + 1U, nullptr) == pdPASS);
    CHECK(peer_ran);
    CHECK(irq_masked());
    taskEXIT_CRITICAL();
    CHECK(irq_masked());
    uni_hal_core_irq_resume(saved);
    CHECK(!irq_masked());

    taskENTER_CRITICAL();
    uint32_t const inner = uni_hal_core_irq_pause();
    uni_hal_core_irq_resume(inner);
    CHECK(irq_masked());
    taskEXIT_CRITICAL();
    CHECK(!irq_masked());

    finish(0U);
}

int main(void) {
    uni_hal_core_zynq7000_init();
    CHECK(xTaskCreate(run, "nesting", 1024U, nullptr, test_priority, nullptr) == pdPASS);
    vTaskStartScheduler();
    finish(200U);
}

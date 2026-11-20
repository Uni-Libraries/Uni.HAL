#include <stm32h7xx_hal.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "flash/uni_hal_flash.h"
#include "runner.h"

static unsigned allocations, releases, irq_enables;
static bool fail, preempt;
static StaticSemaphore_t storage[2];
static SemaphoreHandle_t create(void) {
    ++allocations;
    if (fail) return nullptr;
    if (preempt) {
        preempt = false;
        uni_hal_flash_init(); // Another caller finishes while the first is allocating.
        return (SemaphoreHandle_t)&storage[1];
    }
    return (SemaphoreHandle_t)&storage[0];
}
#undef xSemaphoreCreateBinary
#undef vSemaphoreDelete
#define xSemaphoreCreateBinary() create()
#define vSemaphoreDelete(handle) do { (void)(handle); ++releases; } while (0)
#define HAL_NVIC_SetPriority(irq, priority, group) ((void)0)
#define HAL_NVIC_EnableIRQ(irq) (++irq_enables)
#include "flash/uni_hal_flash_stm32h7.c"

static unsigned check(void) {
    fail = true;
    uni_hal_flash_init();
    CHECK(irq_enables == 0 && allocations == 1);
    fail = false;
    preempt = true;
    uni_hal_flash_init();
    CHECK(allocations == 3 && releases == 1 && irq_enables == 1);
    uni_hal_flash_init();
    CHECK(allocations == 3 && releases == 1 && irq_enables == 1);
    return 0;
}

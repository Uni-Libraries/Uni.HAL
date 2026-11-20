// Drive the Zynq delay with the BSP global counter and an independently phased RTOS tick.
#undef NDEBUG
#include <assert.h>
#include <stdbool.h>
#include <FreeRTOS.h>
#include <task.h>
#define XPAR_CPU_CORE_CLOCK_FREQ_HZ 650000000U
#include <xiltimer.h>

// The Cortex-A9 FreeRTOS port has a 32-bit TickType_t even on an LP64 test host.
#undef portMAX_DELAY
#define portMAX_DELAY ((TickType_t)UINT32_MAX)
#undef configTICK_RATE_HZ
#define configTICK_RATE_HZ TEST_TICK_HZ

static XTime now, until_tick;
static bool running, wake_early;
static unsigned sleeps;
static const XTime tick_counts = (COUNTS_PER_SECOND) / configTICK_RATE_HZ;

void XTime_GetTime(XTime *t) {
    *t = now;
    if (!running) {
        now += (COUNTS_PER_SECOND) / 10000U;
    }
}

static BaseType_t scheduler_state(void) {
    return running ? taskSCHEDULER_RUNNING : taskSCHEDULER_NOT_STARTED;
}

static void model_delay(TickType_t ticks) {
    ++sleeps;
    assert(ticks < portMAX_DELAY);
    if (ticks == 0U) {
        return;
    }
    if (wake_early) {
        wake_early = false;
        return;
    }
    now += until_tick + (ticks - 1ULL) * tick_counts;
    until_tick = tick_counts;
}

#define xTaskGetSchedulerState scheduler_state
#define vTaskDelay model_delay
#include "systick/uni_hal_systick_zynq7000.c"

static void check_delay(uint32_t ms) {
    XTime const start = now;
    until_tick = 1U; // call immediately before the next tick
    sleeps = 0;
    uni_hal_systick_delay(ms);
    XTime const requested = (uint64_t)(ms ? ms : 1U) * ((COUNTS_PER_SECOND) / 1000U);
    assert(now - start >= requested);
    assert(now - start < requested + tick_counts + (COUNTS_PER_SECOND) / 10000U);
    assert((sleeps != 0U) == running);
}

int main(void) {
    running = true;
    check_delay(1U);
    check_delay(0U);
    wake_early = true;
    check_delay(29U);
    now = UINT64_MAX - tick_counts;
    check_delay(UINT32_MAX);
    running = false;
    check_delay(4U);
    return 0;
}

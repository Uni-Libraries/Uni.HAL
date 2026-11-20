#include <stdint.h>

static uint32_t model_tick(void);
#define uni_hal_systick_get_ms model_tick
#include "model.h"
#include "../arm/semihost_test.h"

static uint32_t ms, step_ms;
static unsigned reads, ready_at;

static uint32_t model_tick(void) {
    if (++reads == ready_at) {
        watchdog.SR = 0;
    }
    ms += step_ms;
    return ms;
}

static unsigned check(void) {
    uni_hal_iwdg_context_t ctx = {
        .instance = UNI_HAL_CORE_PERIPH_IWDG_1, .watchdog_prescaler = 32, .watchdog_counter = 319,
    };
    SystemCoreClock = 100000U;
    watchdog.SR = IWDG_SR_RVU;
    // With a stopped SysTick the old loop eventually accepts this very late update.
    ready_at = 50000U;
    CHECK(!uni_hal_iwdg_init(&ctx));
    CHECK(reads < ready_at && watchdog.SR == IWDG_SR_RVU);
    CHECK(uni_hal_iwdg_is_inited(&ctx));
    CHECK(uni_hal_iwdg_reload(&ctx) && watchdog.KR == LL_IWDG_KEY_RELOAD);

    // Normal register synchronization still succeeds before the first tick.
    reads = 0;
    ready_at = 100U;
    CHECK(uni_hal_iwdg_init(&ctx));
    CHECK(reads == ready_at);

    watchdog.SR = IWDG_SR_PVU;
    ms = UINT32_MAX - 50U;
    step_ms = 25U;
    reads = 0;
    CHECK(!uni_hal_iwdg_init(&ctx));
    CHECK(reads < 10U && watchdog.SR == IWDG_SR_PVU);
    return 0;
}

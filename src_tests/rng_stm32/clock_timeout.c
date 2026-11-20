#include "model.h"

static unsigned check(void) {
    uni_hal_rng_context_t ctx = {.instance = UNI_HAL_CORE_PERIPH_RNG, .clock_source = UNI_HAL_RCC_CLKSRC_HSI48};
    SystemCoreClock = 100000U;
    rng_regs.SR = RNG_SR_DRDY;
    // An oscillator that starts very late must not keep initialization waiting for SysTick.
    clock_ready_at = 50000U;
    CHECK(!uni_hal_rng_init(&ctx));
    CHECK(polls < clock_ready_at && !uni_hal_rng_is_inited(&ctx) && !LL_RNG_IsEnabled(RNG));

    polls = 0;
    clock_ready_at = 50U;
    CHECK(uni_hal_rng_init(&ctx) && uni_hal_rng_is_inited(&ctx));
    CHECK(LL_RCC_HSI48_IsReady() && LL_RNG_IsEnabled(RNG));
    return 0;
}

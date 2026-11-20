#include "model.h"

static unsigned check(void) {
    uni_hal_rng_context_t ctx = {.instance = UNI_HAL_CORE_PERIPH_RNG};
    rng_regs.SR = RNG_SR_DRDY;
    CHECK(uni_hal_rng_init(&ctx) && uni_hal_rng_is_inited(&ctx));

    ctx.clock_source = UNI_HAL_RCC_CLKSRC_PCLK1;
    CHECK(!uni_hal_rng_init(&ctx));
    CHECK(!uni_hal_rng_is_inited(&ctx));
    uint8_t value = 0x5AU;
    CHECK(!uni_hal_rng_get(&ctx, &value, sizeof(value)) && value == 0x5AU);
    CHECK(uni_hal_rng_get_32u(&ctx) == 0U);

    ctx.clock_source = UNI_HAL_RCC_CLKSRC_PLL1Q;
    CHECK(uni_hal_rng_init(&ctx) && uni_hal_rng_get_32u(&ctx) == rng_regs.DR);

    // A timeout before clock selection must invalidate the context as well.
    ctx.clock_source = UNI_HAL_RCC_CLKSRC_HSI48;
    step_ms = 25U;
    CHECK(!uni_hal_rng_init(&ctx) && !uni_hal_rng_is_inited(&ctx));
    clock_ready_at = polls + 1U;
    CHECK(uni_hal_rng_init(&ctx) && uni_hal_rng_is_inited(&ctx));
    return 0;
}

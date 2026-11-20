#include "model.h"

static unsigned check(void) {
    uni_hal_rng_context_t ctx = {.instance = UNI_HAL_CORE_PERIPH_RNG, .clock_source = UNI_HAL_RCC_CLKSRC_HSI48};
    SystemCoreClock = 100000U;
    clock_ready_at = 1U;
    word_ready_at = 50000U;
    CHECK(!uni_hal_rng_init(&ctx));
    CHECK(polls < word_ready_at && !uni_hal_rng_is_inited(&ctx));

    word_ready_at = polls + 50U;
    CHECK(uni_hal_rng_init(&ctx));
    rng_regs.SR = 0;
    word_ready_at = polls + 50000U;
    uint8_t value = 0xA5U;
    CHECK(!uni_hal_rng_get(&ctx, &value, 1));
    CHECK(value == 0xA5U && polls < word_ready_at);

    polls = 0;
    ms = UINT32_MAX - 50U;
    step_ms = 25U;
    CHECK(uni_hal_rng_get_32u(&ctx) == 0U && polls < 10U);
    word_ready_at = polls + 2U;
    CHECK(uni_hal_rng_get_32u(&ctx) == rng_regs.DR);
    return 0;
}

#include "tim/uni_hal_tim.h"
#include "../arm/semihost_test.h"

static uint32_t clock_hz;
uint32_t __wrap_uni_hal_rcc_clk_get_freq(uni_hal_core_periph_e periph) {
    return periph == UNI_HAL_CORE_PERIPH_TIM_1 ? clock_hz : 0U;
}

static unsigned check(void) {
    uni_hal_tim_context_t ctx = {.config = {.instance = UNI_HAL_CORE_PERIPH_TIM_1}, .status = {.inited = true}};
    uni_hal_tim_channel_state_t capture = {.period = 80000U, .valid = true};
    clock_hz = 80000000U; // 12.5 ns per tick
    CHECK(uni_hal_tim_get_chan_period_ns(&ctx, &capture) == 1000000U);
    CHECK(uni_hal_tim_get_chan_period_us(&ctx, &capture) == 1000U);
    CHECK(uni_hal_tim_get_chan_freq_mhz(&ctx, &capture) == 1000000U);
    CHECK(uni_hal_tim_get_chan_freq_hz(&ctx, &capture) == 1000U);

    capture.period = 1U;
    CHECK(uni_hal_tim_get_tick_period_ns(&ctx) == 12U);
    CHECK(uni_hal_tim_get_chan_freq_hz(&ctx, &capture) == clock_hz);
    ctx.config.prescaler = UINT16_MAX;
    capture.period = UINT32_MAX; // the intermediate product with 10^9 cannot fit in 64 bits
    CHECK(uni_hal_tim_get_chan_period_ns(&ctx, &capture) == UINT32_MAX * 819200ULL);
    clock_hz = 3000000U;
    CHECK(uni_hal_tim_get_tick_period_ns(&ctx) == 21845333U);
    clock_hz = 1000U;
    CHECK(uni_hal_tim_get_tick_period_ns(&ctx) == 0U);
    CHECK(uni_hal_tim_get_tick_period_us(&ctx) == 65536000U);
    CHECK(uni_hal_tim_get_chan_period_ns(&ctx, &capture) == UINT64_MAX);
    CHECK(uni_hal_tim_get_chan_period_us(&ctx, &capture) == UINT32_MAX * 65536000ULL);
    clock_hz = 0U;
    CHECK(uni_hal_tim_get_chan_period_ns(&ctx, &capture) == UINT64_MAX);
    CHECK(uni_hal_tim_get_chan_freq_mhz(&ctx, &capture) == UINT64_MAX);
    return 0;
}

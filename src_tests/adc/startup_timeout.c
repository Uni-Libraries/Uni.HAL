#include "model.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

static uni_hal_adc_context_t context(uint32_t timeout) {
    return (uni_hal_adc_context_t){.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_1, .channels_count = 1U, .channels = {1U},
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
        .clock_source = UNI_HAL_RCC_CLKSRC_PLL2P,
#endif
        .timeout = timeout
    }};
}

static unsigned check_timeout(bool ready, uint32_t timeout, uint32_t step, unsigned expected_reads) {
    reset_adc_model();
    uni_hal_adc_context_t ctx = context(timeout);
    unsigned *delay = ready ? &adc_ready_delay : &adc_calibration_delay;
    // The flag changes later even if the driver misses its deadline. A broken wait can't hang the test.
    *delay = 7U;
    adc_tick_step = step;
    CHECK(!uni_hal_adc_init(&ctx));
    CHECK(!uni_hal_adc_is_inited(&ctx));
    CHECK(*delay == 7U - expected_reads);
    CHECK(!(adc_model_registers(ctx.config.instance)->CR & ADC_CR_ADSTART));
    return 0;
}

unsigned test_adc(void) {
    for (unsigned stage = 0; stage < 2U; ++stage) {
        // Two observations span 2^32 ms. The clock skips UINT32_MAX on both stages.
        unsigned result = check_timeout(stage != 0U, UINT32_MAX, 1UL << 31U, 3U);
        if (result) return result;
        // One starting sample and three ticks until the configured limit.
        result = check_timeout(stage != 0U, 3U, 1U, 4U);
        if (result) return result;
        // Zero selects the default 500 ms for either phase.
        result = check_timeout(stage != 0U, 0U, 125U, 5U);
        if (result) return result;
    }
    // Each stage can finish on the last tick of its own budget.
    reset_adc_model();
    adc_calibration_delay = adc_ready_delay = 3U;
    uni_hal_adc_context_t ctx = context(3U);
    CHECK(uni_hal_adc_init(&ctx));
    CHECK(uni_hal_adc_is_inited(&ctx));
    return 0;
}

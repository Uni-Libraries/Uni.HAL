#include "model.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

static unsigned check_stage(unsigned *delay) {
    reset_adc_model();
    SystemCoreClock = 100000U;
    adc_tick_step = 0;
    uni_hal_adc_context_t ctx = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_1, .channels_count = 1U, .channels = {1U}, .timeout = 2U,
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
        .clock_source = UNI_HAL_RCC_CLKSRC_PLL2P,
#endif
    }};

    // Let the hardware complete late so a driver waiting only for SysTick can return.
    *delay = 2000U;
    CHECK(!uni_hal_adc_init(&ctx));
    CHECK(*delay > 0U && !uni_hal_adc_is_inited(&ctx));
    CHECK(!(adc_model_registers(ctx.config.instance)->CR & ADC_CR_ADSTART));

    reset_adc_model();
    adc_tick_step = 0;
    *delay = 10U;
    CHECK(uni_hal_adc_init(&ctx));
    CHECK(uni_hal_adc_is_inited(&ctx));
    return 0U;
}

unsigned test_adc(void) {
    unsigned line = check_stage(&adc_calibration_delay);
    return line ? line : check_stage(&adc_ready_delay);
}

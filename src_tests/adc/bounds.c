#include "model.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

static bool rejected(uni_hal_adc_context_t *ctx, unsigned expected_dma_calls) {
    reset_adc_model();
    return !uni_hal_adc_init(ctx) && !uni_hal_adc_is_inited(ctx) &&
           adc_model_untouched() && dma_init_calls == expected_dma_calls;
}

unsigned test_adc(void) {
    uint16_t samples[UNI_HAL_ADC_CHANNELS_MAX + 1U] = {};
    uni_hal_adc_context_t ctx = {.config = {.instance = UNI_HAL_CORE_PERIPH_ADC_1}};
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    ctx.config.clock_source = UNI_HAL_RCC_CLKSRC_PLL2P;
#endif
    CHECK(rejected(nullptr, 0));
    CHECK(rejected(&ctx, 0));
    ctx.config.channels_count = 1U;
    CHECK(uni_hal_adc_init(&ctx));
    CHECK(uni_hal_adc_is_inited(&ctx));
    CHECK(uni_hal_adc_get_rank_raw(&ctx, 0) == UINT16_MAX); // Direct register mode, no DMA buffer.

    reset_adc_model();
    ctx.state = (uni_hal_adc_state_t){};
    ctx.config.channels_count = UNI_HAL_ADC_CHANNELS_MAX;
    ctx.config.data = samples;
    for (unsigned i = 0; i < UNI_HAL_ADC_CHANNELS_MAX; ++i) {
        ctx.config.channels[i] = i;
        samples[i] = i + 1U;
    }
    CHECK(uni_hal_adc_init(&ctx));
    CHECK(uni_hal_adc_get_rank_raw(&ctx, 15U) == 16U);
    CHECK(uni_hal_adc_get_rank_raw(&ctx, 16U) == UINT16_MAX);

    // Caller-owned configurations can also change after a successful init.
    ctx.config.channels_count++;
    CHECK(uni_hal_adc_get_rank_raw(&ctx, 16U) == UINT16_MAX);
    ctx.state = (uni_hal_adc_state_t){}; // Fresh context to test cold initialization.
    CHECK(rejected(&ctx, 0));
    ctx.config.channels_count = 1U;
    ctx.config.instance = UNI_HAL_CORE_PERIPH_UNKNOWN;
    CHECK(rejected(&ctx, 0));
    ctx.config.instance = UNI_HAL_CORE_PERIPH_ADC_1;

    uni_hal_gpio_pin_context_t pin = {};
    ctx.config.pins[0] = &pin;
    ctx.config.channels[0] = UINT32_MAX;
    CHECK(rejected(&ctx, 0));
    ctx.config.channels[0] = 0U;
    ctx.config.resolution_bits = 7U;
    CHECK(rejected(&ctx, 0));
    ctx.config.resolution_bits = 0U;
    ctx.config.sampling_cycles = UINT32_MAX;
    CHECK(rejected(&ctx, 0));
    ctx.config.sampling_cycles = 0U;
    ctx.config.clock_divider = 3U;
    CHECK(rejected(&ctx, 0));
    ctx.config.clock_divider = 0U;
    const uni_hal_rcc_clksrc_e good_source = ctx.config.clock_source;
    ctx.config.clock_source = UNI_HAL_RCC_CLKSRC_LSE;
    CHECK(rejected(&ctx, 0));
    ctx.config.clock_source = good_source;

    uni_hal_dma_context_t dma = {.config = {.instance = UNI_HAL_CORE_PERIPH_DMA_1, .channel = UNI_HAL_DMA_CHANNEL_1}};
    ctx.config.dma = &dma;
    ctx.config.data = nullptr;
    CHECK(rejected(&ctx, 0));
    alignas(uint16_t) uint8_t unaligned[3];
    ctx.config.data = (uint16_t *)(unaligned + 1);
    CHECK(rejected(&ctx, 0));
    ctx.config.data = samples;
    dma.config.instance = UNI_HAL_CORE_PERIPH_ADC_1;
    CHECK(rejected(&ctx, 0));
    dma.config.instance = UNI_HAL_CORE_PERIPH_DMA_2;
    dma.config.channel = (uni_hal_dma_channel_e)8;
    CHECK(rejected(&ctx, 0));
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    dma.config.channel = UNI_HAL_DMA_CHANNEL_0;
    CHECK(rejected(&ctx, 0));
#endif
    dma.config.channel = UNI_HAL_DMA_CHANNEL_3;
    CHECK(rejected(&ctx, 1)); // A valid DMA descriptor whose initialization fails.
    reset_adc_model();
    adc_dma_init_succeeds = true;
    dma.config.instance = UNI_HAL_CORE_PERIPH_DMA_1;
    dma.config.channel = UNI_HAL_DMA_CHANNEL_1;
    CHECK(uni_hal_adc_init(&ctx));
    CHECK(adc_model_capturing(samples, 1U));
    const unsigned calls_after_start = adc_peripheral_calls;
    CHECK(uni_hal_adc_init(&ctx) && dma_init_calls == 1U && adc_peripheral_calls == calls_after_start);
    CHECK(adc_model_capturing(samples, 1U));

    ctx.config.channels_count = 0U;
    CHECK(!uni_hal_adc_init(&ctx) && uni_hal_adc_is_inited(&ctx));
    CHECK(adc_model_capturing(samples, 1U) && dma_init_calls == 1U);
    CHECK(adc_peripheral_calls == calls_after_start);
    return 0;
}

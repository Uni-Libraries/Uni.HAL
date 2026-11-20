#include "model.h"
#include <stm32h7xx_ll_adc.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

static uni_hal_adc_context_t *during_setup;
static bool nested_started;

ErrorStatus __real_LL_ADC_Init(ADC_TypeDef *adc, LL_ADC_InitTypeDef *config);
ErrorStatus __wrap_LL_ADC_Init(ADC_TypeDef *adc, LL_ADC_InitTypeDef *config) {
    if (during_setup) {
        uni_hal_adc_context_t *other = during_setup;
        during_setup = nullptr;
        nested_started = uni_hal_adc_init(other);
    }
    return __real_LL_ADC_Init(adc, config);
}

unsigned test_adc(void) {
    reset_adc_model();
    adc_dma_init_succeeds = true;
    uint16_t samples[2] = {110U, 220U}, other_sample = 330U;
    uni_hal_dma_context_t dma = {.config = {.instance = UNI_HAL_CORE_PERIPH_DMA_1, .channel = UNI_HAL_DMA_CHANNEL_1}};
    uni_hal_dma_context_t other_dma = {.config = {.instance = UNI_HAL_CORE_PERIPH_DMA_1, .channel = UNI_HAL_DMA_CHANNEL_2}};
    uni_hal_adc_context_t owner = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_1, .clock_source = UNI_HAL_RCC_CLKSRC_PLL2P,
        .dma = &dma, .data = samples, .channels_count = 2U, .channels = {1U, 2U}
    }};
    uni_hal_adc_context_t other = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_1, .clock_source = UNI_HAL_RCC_CLKSRC_PLL2P,
        .dma = &other_dma, .data = &other_sample, .channels_count = 1U, .channels = {3U}
    }};

    CHECK(uni_hal_adc_init(&owner));
    const ADC_TypeDef *adc = adc_model_registers(owner.config.instance);
    uint32_t const sequence = adc->SQR1;
    CHECK(!uni_hal_adc_init(&other) && !uni_hal_adc_is_inited(&other));
    CHECK(adc->SQR1 == sequence && adc_model_capturing(samples, 2U));
    CHECK(uni_hal_adc_init(&owner) && uni_hal_adc_get_rank_raw(&owner, 1U) == samples[1]);
    other.config.dma = nullptr;
    CHECK(!uni_hal_adc_init(&other));
    CHECK(adc->SQR1 == sequence && adc_model_capturing(samples, 2U));

    // ADC2 shares ADC1's clock but can start independently on the unused stream.
    other.config.instance = UNI_HAL_CORE_PERIPH_ADC_2;
    other.config.dma = &other_dma;
    uni_hal_adc_context_t contender = {.config = other.config};
    contender.config.dma = nullptr;
    during_setup = &contender;
    nested_started = true;
    CHECK(uni_hal_adc_init(&other));
    CHECK(!during_setup && !nested_started && !uni_hal_adc_is_inited(&contender));
    CHECK(uni_hal_adc_get_rank_raw(&other, 0U) == other_sample);
    CHECK(adc->SQR1 == sequence && adc_model_capturing(samples, 2U));
    return 0U;
}

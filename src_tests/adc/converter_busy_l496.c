#include <stm32l4xx.h>

// Exercise the shared gate and clock selector through the production RCC driver.
static RCC_TypeDef rcc;
#undef RCC
#define RCC (&rcc)
#include "rcc/uni_hal_rcc_stm32l4.c"
#include "model.h"
#include <stm32l4xx_ll_dma.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

unsigned test_adc(void) {
    reset_adc_model();
    g_uni_hal_rcc_status.inited = true; // System clock startup has completed.
    adc_dma_init_succeeds = true;
    uint16_t samples[2] = {110U, 220U}, other_sample = 330U;
    // Both DMA1 channel 1 and DMA2 channel 3 are valid ADC1 routes on L496.
    uni_hal_dma_context_t dma = {.config = {.instance = UNI_HAL_CORE_PERIPH_DMA_1, .channel = UNI_HAL_DMA_CHANNEL_1}};
    uni_hal_dma_context_t other_dma = {.config = {.instance = UNI_HAL_CORE_PERIPH_DMA_2, .channel = UNI_HAL_DMA_CHANNEL_3}};
    uni_hal_adc_context_t owner = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_1, .dma = &dma, .data = samples, .channels_count = 2U, .channels = {1U, 2U}
    }};
    uni_hal_adc_context_t other = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_1, .clock_source = UNI_HAL_RCC_CLKSRC_NONE,
        .dma = &other_dma, .data = &other_sample, .channels_count = 1U, .channels = {3U}
    }};

    CHECK(uni_hal_adc_init(&owner));
    const ADC_TypeDef *adc = adc_model_registers(owner.config.instance);
    uint32_t const sequence = adc->SQR1;
    CHECK((rcc.CCIPR & RCC_CCIPR_ADCSEL) == LL_RCC_ADC_CLKSOURCE_SYSCLK);
    CHECK(!uni_hal_adc_init(&other) && !uni_hal_adc_is_inited(&other));
    CHECK((rcc.CCIPR & RCC_CCIPR_ADCSEL) == LL_RCC_ADC_CLKSOURCE_SYSCLK);
    CHECK(!LL_DMA_IsEnabledChannel(uni_hal_dma_stm32l4_get_module(other_dma.config.instance), LL_DMA_CHANNEL_3));
    CHECK(adc->SQR1 == sequence && adc_model_capturing(samples, 2U));
    other.config.dma = nullptr;
    CHECK(!uni_hal_adc_init(&other) && uni_hal_adc_get_rank_raw(&other, 0U) == UINT16_MAX);
    CHECK((rcc.CCIPR & RCC_CCIPR_ADCSEL) == LL_RCC_ADC_CLKSOURCE_SYSCLK && adc->SQR1 == sequence);
    CHECK(uni_hal_adc_init(&owner) && uni_hal_adc_get_rank_raw(&owner, 1U) == samples[1]);

    uni_hal_adc_context_t independent = {.config = other.config};
    independent.config.instance = UNI_HAL_CORE_PERIPH_ADC_2;
    independent.config.clock_source = UNI_HAL_RCC_CLKSRC_SYSCLK;
    CHECK(uni_hal_adc_init(&independent));
    CHECK(adc_model_registers(independent.config.instance)->CR & ADC_CR_ADSTART);
    CHECK(adc->SQR1 == sequence && adc_model_capturing(samples, 2U));

    // Once hardware is reset, the rejected attempt must not keep the ADC1 setup reservation.
    reset_adc_model();
    other.config.clock_source = UNI_HAL_RCC_CLKSRC_SYSCLK;
    CHECK(uni_hal_adc_init(&other) && uni_hal_adc_is_inited(&other));
    CHECK(adc->CR & ADC_CR_ADSTART);
    return 0U;
}

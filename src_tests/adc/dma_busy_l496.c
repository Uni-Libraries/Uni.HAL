#include "model.h"
#include <stm32l4xx_ll_dma.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

static volatile uint16_t first[2], second[1];
static uni_hal_dma_context_t dma = {.config = {.instance = UNI_HAL_CORE_PERIPH_DMA_1, .channel = UNI_HAL_DMA_CHANNEL_1}};
static uni_hal_dma_context_t peer = {.config = {.instance = UNI_HAL_CORE_PERIPH_DMA_1, .channel = UNI_HAL_DMA_CHANNEL_1}};
static uni_hal_adc_context_t owner = {.config = {
    .instance = UNI_HAL_CORE_PERIPH_ADC_1, .dma = &dma, .data = first, .channels_count = 2U, .channels = {1U, 2U}
}};
static uni_hal_adc_context_t contender = {.config = {
    .instance = UNI_HAL_CORE_PERIPH_ADC_1, .dma = &peer, .data = second, .channels_count = 1U, .channels = {3U}
}};
static uni_hal_adc_context_t direct = {.config = {
    .instance = UNI_HAL_CORE_PERIPH_ADC_1, .channels_count = 1U, .channels = {4U}
}};
static bool nested, direct_started;

static void start_adc(void) {
    nested = uni_hal_adc_init(&contender);
}

static void start_other_transfer(void) {
    direct_started = uni_hal_adc_init(&direct);
    nested = uni_hal_dma_init(&peer) &&
             uni_hal_dma_set_request(&peer) &&
             uni_hal_dma_set_address(&peer, 0x40000000U, (uint32_t)second) &&
             uni_hal_dma_set_length(&peer, 1U) &&
             uni_hal_dma_enable(&peer, true);
}

unsigned test_adc(void) {
    reset_adc_model();
    adc_dma_init_succeeds = true;
    CHECK(uni_hal_adc_init(&owner));
    uint32_t const sequence = adc_model_registers(UNI_HAL_CORE_PERIPH_ADC_1)->SQR1;
    CHECK(!uni_hal_adc_init(&contender) && !uni_hal_adc_is_inited(&contender));
    CHECK(adc_model_registers(UNI_HAL_CORE_PERIPH_ADC_1)->SQR1 == sequence);
    CHECK(adc_model_capturing(first, 2U) && uni_hal_adc_init(&owner));

    reset_adc_model();
    owner.state.initialized = false;
    adc_dma_init_succeeds = true;
    adc_delay_hook = start_adc;
    nested = true;
    CHECK(uni_hal_adc_init(&owner));
    CHECK(!nested && adc_model_capturing(first, 2U));

    reset_adc_model();
    owner.state.initialized = false;
    adc_dma_init_succeeds = true;
    adc_delay_hook = start_other_transfer;
    nested = false;
    CHECK(!uni_hal_adc_init(&owner) && nested);
    CHECK(!direct_started && !uni_hal_adc_is_inited(&direct));
    const DMA_TypeDef *module = adc_model_dma1();
    CHECK(LL_DMA_IsEnabledChannel(module, LL_DMA_CHANNEL_1));
    CHECK(LL_DMA_GetPeriphRequest(module, LL_DMA_CHANNEL_1) == LL_DMA_REQUEST_4);
    CHECK(LL_DMA_GetMemoryAddress(module, LL_DMA_CHANNEL_1) == (uintptr_t)second);
    CHECK(LL_DMA_GetDataLength(module, LL_DMA_CHANNEL_1) == 1U);
    const ADC_TypeDef *adc = adc_model_registers(UNI_HAL_CORE_PERIPH_ADC_1);
    CHECK(!(adc->CR & (ADC_CR_ADSTART | ADC_CR_ADEN | ADC_CR_ADDIS)) && !(adc->ISR & ADC_ISR_ADRDY));
    CHECK(uni_hal_dma_enable(&peer, false));
    CHECK(uni_hal_adc_init(&owner) && adc_model_capturing(first, 2U));
    return 0;
}

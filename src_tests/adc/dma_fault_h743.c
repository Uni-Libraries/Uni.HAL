#include "model.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

static bool inject_direct_mode_fault;
static bool fault_acknowledged;

void __real_uni_hal_core_irq_resume(uint32_t mask);
void __wrap_uni_hal_core_irq_resume(uint32_t mask) {
    __real_uni_hal_core_irq_resume(mask);
    if (inject_direct_mode_fault && mask == 0U) {
        inject_direct_mode_fault = false;
        fault_acknowledged = adc_model_dma_fault(true);
    }
}

unsigned test_adc(void) {
    reset_adc_model();
    adc_dma_init_succeeds = true;
    uni_hal_dma_context_t dma = {.config = {.instance = UNI_HAL_CORE_PERIPH_DMA_1, .channel = UNI_HAL_DMA_CHANNEL_1}};
    uint16_t first_sample = 101U, second_sample = 202U;
    uni_hal_adc_context_t first = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_1, .clock_source = UNI_HAL_RCC_CLKSRC_PLL2P,
        .channels_count = 1U, .channels = {1U}, .data = &first_sample, .dma = &dma, .v_ref = 3300U
    }};
    CHECK(uni_hal_adc_init(&first));
    CHECK(uni_hal_adc_get_channel_raw(&first, 1U) == first_sample);
    first_sample = UINT16_MAX;
    CHECK(uni_hal_adc_get_channel_mv(&first, 1U) == 3300U);
    bool const stopped = adc_model_dma_fault(false);
    CHECK(!uni_hal_adc_is_inited(&first));
    CHECK(stopped && !uni_hal_adc_init(&first));
    CHECK(uni_hal_adc_get_rank_raw(&first, 0U) == UINT16_MAX);
    CHECK(!uni_hal_adc_has_channel(&first, 1U));
    CHECK(uni_hal_adc_get_channel_mv(&first, 1U) == UINT16_MAX);
    CHECK(uni_hal_dma_enable(&dma, false));

    // Even reusing the same DMA descriptor must not republish the first ADC's old buffer.
    uni_hal_adc_context_t second = {.config = first.config};
    second.config.instance = UNI_HAL_CORE_PERIPH_ADC_2;
    second.config.data = &second_sample;
    CHECK(uni_hal_adc_init(&second));
    CHECK(uni_hal_adc_get_rank_raw(&second, 0U) == second_sample);
    CHECK(uni_hal_adc_get_channel_raw(&first, 1U) == UINT16_MAX);
    CHECK(!uni_hal_adc_init(&first));
    // A fault after the readiness check must not turn an unavailable sample into full-scale voltage.
    uint16_t const expected_mv = uni_hal_adc_get_channel_mv(&second, 1U);
    inject_direct_mode_fault = true;
    uint16_t const reading = uni_hal_adc_get_channel_mv(&second, 1U);
    CHECK(fault_acknowledged && (reading == expected_mv || reading == UINT16_MAX));
    CHECK(!uni_hal_adc_is_inited(&second));
    CHECK(uni_hal_adc_get_channel_raw(&second, 1U) == UINT16_MAX);
    CHECK(uni_hal_dma_enable(&dma, false));

    uni_hal_adc_context_t third = {.config = first.config};
    third.config.instance = UNI_HAL_CORE_PERIPH_ADC_3;
    CHECK(uni_hal_adc_init(&third));
    CHECK(uni_hal_dma_enable(&dma, false));
    CHECK(!uni_hal_adc_is_inited(&third));
    CHECK(uni_hal_dma_enable(&dma, true)); // A raw DMA transfer, without an ADC owner.
    CHECK(!uni_hal_adc_is_inited(&third));
    return 0;
}

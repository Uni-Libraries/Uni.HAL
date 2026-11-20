#include "model.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

static unsigned check_order(bool adc1_first) {
    // RM0351: VREFINT is connected to ADC1, temperature and VBAT to ADC1 and ADC3.
    uni_hal_adc_context_t reference = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_1, .clock_divider = 4U,
        .channels = {UNI_HAL_ADC_STM32L4_CHANNEL_REFINT}, .channels_count = 1U,
    }};
    uni_hal_adc_context_t sensors = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_3, .clock_divider = 8U,
        .channels = {UNI_HAL_ADC_STM32L4_CHANNEL_TEMPSENSOR, UNI_HAL_ADC_STM32L4_CHANNEL_VBAT},
        .channels_count = 2U,
    }};
    uni_hal_adc_context_t *first = adc1_first ? &reference : &sensors;
    uni_hal_adc_context_t *second = adc1_first ? &sensors : &reference;
    uint32_t const first_paths = adc1_first ? ADC_CCR_VREFEN : (ADC_CCR_TSEN | ADC_CCR_VBATEN);
    uint32_t const all_paths = ADC_CCR_VREFEN | ADC_CCR_TSEN | ADC_CCR_VBATEN;

    reset_adc_model();
    // Firmware before us may have left unused paths enabled with all ADCs off.
    ADC_Common_TypeDef *common = adc_model_common_registers();
    common->CCR = all_paths;
    CHECK(uni_hal_adc_init(first));
    const ADC_TypeDef *running = adc_model_registers(first->config.instance);
    uint32_t const first_ccr = common->CCR;
    uint32_t const first_sequence = running->SQR1;
    uint32_t const converting = ADC_CR_ADEN | ADC_CR_ADSTART;
    CHECK((first_ccr & all_paths) == first_paths && (running->CR & converting) == converting);
    CHECK(uni_hal_adc_init(second));
    CHECK(common->CCR == (first_ccr | all_paths));
    CHECK((running->CR & converting) == converting && running->SQR1 == first_sequence);
    CHECK((adc_model_registers(second->config.instance)->CR & converting) == converting);
    return 0U;
}

unsigned test_adc(void) {
    unsigned failed = check_order(true);
    return failed ? failed : check_order(false);
}

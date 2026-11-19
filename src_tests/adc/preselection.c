#include "model.h"

unsigned test_adc(void) {
    // The second input is repeated in the sequence. PCSEL has one bit for each distinct channel.
    uni_hal_adc_context_t ctx = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_1,
        .clock_source = UNI_HAL_RCC_CLKSRC_PLL2P,
        .channels = {2U, 6U, 6U, 19U},
        .channels_count = 4U,
    }};
    reset_adc_model();
    if (!uni_hal_adc_init(&ctx)) {
        return __LINE__;
    }
    const ADC_TypeDef *adc = adc_model_registers();
    const uint32_t sequence = (3U << ADC_SQR1_L_Pos) | (2U << ADC_SQR1_SQ1_Pos) |
                              (6U << ADC_SQR1_SQ2_Pos) | (6U << ADC_SQR1_SQ3_Pos) |
                              (19U << ADC_SQR1_SQ4_Pos);
    if (adc->SQR1 != sequence) {
        return __LINE__;
    }
    return adc->PCSEL == ((1U << 2) | (1U << 6) | (1U << 19)) ? 0U : __LINE__;
}

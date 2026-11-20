#pragma once
#include "adc/uni_hal_adc.h"

void reset_adc_model(void);
bool adc_model_untouched(void);
extern unsigned adc_peripheral_calls;
const ADC_TypeDef *adc_model_registers(void);
bool adc_model_capturing(const volatile uint16_t *data, uint32_t count);
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
bool adc_model_dma_fault(bool direct_mode);
#endif
extern bool adc_dma_init_succeeds;
extern unsigned dma_init_calls;
unsigned test_adc(void);

#pragma once
#include "adc/uni_hal_adc.h"

void reset_adc_model(void);
bool adc_model_untouched(void);
extern unsigned adc_peripheral_calls;
bool adc_model_capturing(const volatile uint16_t *data, uint32_t count);
extern bool adc_dma_init_succeeds;
extern unsigned dma_init_calls;
unsigned test_adc(void);

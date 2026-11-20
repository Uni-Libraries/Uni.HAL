#pragma once
#include "adc/uni_hal_adc.h"

void reset_adc_model(void);
bool adc_model_untouched(void);
extern unsigned adc_peripheral_calls;
extern unsigned adc_calibration_delay, adc_ready_delay;
extern uint32_t adc_tick_step;
const ADC_TypeDef *adc_model_registers(uni_hal_core_periph_e instance);
ADC_Common_TypeDef *adc_model_common_registers(void);
bool adc_model_capturing(const volatile uint16_t *data, uint32_t count);
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
bool adc_model_dma_fault(bool direct_mode);
#endif
extern bool adc_dma_init_succeeds;
extern unsigned dma_init_calls;
unsigned test_adc(void);

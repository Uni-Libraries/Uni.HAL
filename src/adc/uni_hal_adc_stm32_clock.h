#pragma once

#include "uni_hal_target.h"
#include "adc/uni_hal_adc.h"
#include "rcc/uni_hal_rcc.h"
#if defined(UNI_HAL_TARGET_STM32H7)
#include <stm32h7xx_ll_adc.h>
#else
#include <stm32l4xx_ll_adc.h>
#endif

typedef enum {
    ADC_CLOCK_FREE,
    ADC_CLOCK_BUS,
    ADC_CLOCK_KERNEL,
} adc_clock_use_t;

// The reservation lasts from clock selection through the start of conversion. Afterwards
// the converter's start bits and its common clock register identify active kernel users.
static adc_clock_use_t starting_adc[3];

// Called with interrupts masked. Each family has one RCC ADCSEL, including H743/H753 ADC3
// which has a separate bus gate and a separate common ADC clock divider.
static adc_clock_use_t reserve_adc_clock(const uni_hal_adc_config_t *config, uni_hal_rcc_clksrc_e source) {
    ADC_TypeDef *const converters[] = {ADC1, ADC2, ADC3};
    unsigned const index = config->instance - UNI_HAL_CORE_PERIPH_ADC_1;
    ADC_TypeDef *adc = converters[index];
    if (!uni_hal_rcc_clk_set(config->instance, true) || LL_ADC_REG_IsConversionOngoing(adc)) {
        return ADC_CLOCK_FREE;
    }

    const ADC_Common_TypeDef *block = __LL_ADC_COMMON_INSTANCE(adc);
    adc_clock_use_t use = config->clock_mode == UNI_HAL_ADC_CLOCK_SYNC ? ADC_CLOCK_BUS : ADC_CLOCK_KERNEL;
    if (__LL_ADC_IS_ENABLED_ALL_COMMON_INSTANCE(block)) {
        // configure_common preserves CKMODE while a converter in that block is enabled.
        use = (LL_ADC_GetCommonClock(block) & ADC_CCR_CKMODE) ? ADC_CLOCK_BUS : ADC_CLOCK_KERNEL;
    }
    bool kernel_busy = false;
    for (unsigned i = 0; i < 3U; ++i) {
        if (i == index) {
            continue;
        }
        ADC_TypeDef *peer = converters[i];
        const ADC_Common_TypeDef *peer_common = __LL_ADC_COMMON_INSTANCE(peer);
        adc_clock_use_t peer_use = starting_adc[i];
        if (peer_use != ADC_CLOCK_FREE) {
            if (peer_common == block) {
                // Before ADEN, a nested init could overwrite the first divider and analog paths.
                return ADC_CLOCK_FREE;
            }
        } else if (uni_hal_rcc_clk_get(UNI_HAL_CORE_PERIPH_ADC_1 + i) &&
                   (peer->CR & (ADC_CR_ADSTART | ADC_CR_JADSTART))) {
            peer_use = (LL_ADC_GetCommonClock(peer_common) & ADC_CCR_CKMODE) ? ADC_CLOCK_BUS : ADC_CLOCK_KERNEL;
        }
        kernel_busy |= peer_use == ADC_CLOCK_KERNEL;
    }

    if (kernel_busy) {
        if (use == ADC_CLOCK_KERNEL && uni_hal_rcc_clksrc_get(config->instance) != source) {
            return ADC_CLOCK_FREE;
        }
    } else if (!uni_hal_rcc_clksrc_set(config->instance, source)) {
        return ADC_CLOCK_FREE;
    }
    return use;
}

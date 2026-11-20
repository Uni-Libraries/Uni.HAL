#pragma once

#include "rcc/uni_hal_rcc.h"
#if defined(STM32L496xx)
#include <stm32l4xx_ll_pwr.h>
#include <stm32l4xx_ll_rcc.h>
#else
#include <stm32h7xx_ll_pwr.h>
#include <stm32h7xx_ll_rcc.h>
#endif

// Return the incoming DBP state so callers can restore the write protection.
static inline bool open_backup_access(void) {
    bool const was_open = LL_PWR_IsEnabledBkUpAccess() != 0U;
    if (!was_open) {
        LL_PWR_EnableBkUpAccess();
    }
    return was_open;
}

static inline bool set_rtc_clock_enabled(bool enabled) {
    uint32_t const mask = uni_hal_core_irq_pause();
    bool matches = (LL_RCC_IsEnabledRTC() != 0U) == enabled;
    if (!matches) {
        bool const was_open = open_backup_access();
        if (LL_PWR_IsEnabledBkUpAccess()) {
            if (enabled) {
                LL_RCC_EnableRTC();
            } else {
                LL_RCC_DisableRTC();
            }
            matches = (LL_RCC_IsEnabledRTC() != 0U) == enabled;
        }
        if (!was_open) {
            LL_PWR_DisableBkUpAccess();
        }
    }
    uni_hal_core_irq_resume(mask);
    return matches;
}

static inline bool select_rtc_source(uni_hal_rcc_clksrc_e source) {
    uint32_t selector;
    switch (source) {
        case UNI_HAL_RCC_CLKSRC_NONE: selector = LL_RCC_RTC_CLKSOURCE_NONE; break;
        case UNI_HAL_RCC_CLKSRC_LSE: selector = LL_RCC_RTC_CLKSOURCE_LSE; break;
        case UNI_HAL_RCC_CLKSRC_LSI: selector = LL_RCC_RTC_CLKSOURCE_LSI; break;
#if defined(STM32L496xx)
        case UNI_HAL_RCC_CLKSRC_HSE_DIV_32: selector = LL_RCC_RTC_CLKSOURCE_HSE_DIV32; break;
#else
        case UNI_HAL_RCC_CLKSRC_HSE: selector = LL_RCC_RTC_CLKSOURCE_HSE; break;
#endif
        default: return false;
    }

    uint32_t const mask = uni_hal_core_irq_pause();
    bool selected = LL_RCC_GetRTCClockSource() == selector;
    if (!selected) {
        bool const was_open = open_backup_access();
        if (LL_PWR_IsEnabledBkUpAccess()) {
            LL_RCC_SetRTCClockSource(selector);
            // RTCSEL normally remains locked until a backup reset. LSE failure can unlock it.
            selected = LL_RCC_GetRTCClockSource() == selector;
        }
        if (!was_open) {
            LL_PWR_DisableBkUpAccess();
        }
    }
    uni_hal_core_irq_resume(mask);
    return selected;
}

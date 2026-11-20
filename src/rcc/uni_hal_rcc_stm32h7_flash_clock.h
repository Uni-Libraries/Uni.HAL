#pragma once

#include <stm32h7xx_ll_rcc.h>

// An upper bound in whole hertz keeps a fractional PLL clock above a Flash timing threshold.
static inline uint32_t uni_hal_rcc_pll1p_hz_ceil(void) {
    uint32_t const m = LL_RCC_PLL1_GetM();
    if (m == 0U || !LL_RCC_PLL1P_IsEnabled()) {
        return 0U;
    }

    uint32_t input_hz;
    switch (LL_RCC_PLL_GetSource()) {
        case LL_RCC_PLLSOURCE_HSE:
            input_hz = LL_RCC_HSE_IsReady() ? HSE_VALUE : 0U;
            break;
        case LL_RCC_PLLSOURCE_CSI:
            input_hz = LL_RCC_CSI_IsReady() ? CSI_VALUE : 0U;
            break;
        case LL_RCC_PLLSOURCE_HSI:
            input_hz = LL_RCC_HSI_IsReady() ? HSI_VALUE >> (LL_RCC_HSI_GetDivider() >> RCC_CR_HSIDIV_Pos) : 0U;
            break;
        default:
            return 0U;
    }

    uint32_t const fracn = LL_RCC_PLL1FRACN_IsEnabled() ? LL_RCC_PLL1_GetFRACN() : 0U;
    uint32_t const multiplier_q13 = (LL_RCC_PLL1_GetN() << 13U) + fracn;
    uint32_t const divisor = (m * LL_RCC_PLL1_GetP()) << 13U;
    uint64_t const hz = ((uint64_t)input_hz * multiplier_q13 + divisor - 1U) / divisor;
    return hz > UINT32_MAX ? UINT32_MAX : (uint32_t)hz;
}

static inline uint32_t uni_hal_rcc_axi_hz_ceil(uint32_t sysclk_hz, uint32_t sys_div, uint32_t ahb_div) {
    if (sysclk_hz == 0U) {
        return 0U;
    }
    // ceil(f / d) = 1 + floor((ceil(f) - 1) / d), including both bus dividers.
    uint32_t const core_hz_minus_one = LL_RCC_CALC_SYSCLK_FREQ(sysclk_hz - 1U, sys_div);
    return 1U + LL_RCC_CALC_HCLK_FREQ(core_hz_minus_one, ahb_div);
}

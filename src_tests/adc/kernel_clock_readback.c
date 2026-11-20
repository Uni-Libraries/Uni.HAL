#include "rcc_model.h"
#include "model.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

unsigned test_adc(void) {
    static const uni_hal_rcc_clksrc_e sources[] = {
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
        UNI_HAL_RCC_CLKSRC_PLL2P, UNI_HAL_RCC_CLKSRC_PLL3R, UNI_HAL_RCC_CLKSRC_CLKP,
#else
        UNI_HAL_RCC_CLKSRC_NONE, UNI_HAL_RCC_CLKSRC_SYSCLK,
#endif
    };
    g_uni_hal_rcc_status.inited = true;
    for (size_t i = 0; i < sizeof sources / sizeof sources[0]; ++i) {
        CHECK(uni_hal_rcc_clksrc_set(UNI_HAL_CORE_PERIPH_ADC_2, sources[i]));
        CHECK(uni_hal_rcc_clksrc_get(UNI_HAL_CORE_PERIPH_ADC_1) == sources[i]);
        CHECK(uni_hal_rcc_clksrc_get(UNI_HAL_CORE_PERIPH_ADC_2) == sources[i]);
        CHECK(uni_hal_rcc_clksrc_get(UNI_HAL_CORE_PERIPH_ADC_3) == sources[i]);
    }
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    LL_RCC_SetADCClockSource(RCC_D3CCIPR_ADCSEL); // reserved
#else
    LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_PLLSAI1);
    CHECK(uni_hal_rcc_clksrc_get(UNI_HAL_CORE_PERIPH_ADC_1) == UNI_HAL_RCC_CLKSRC_UNKNOWN);
    LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_PLLSAI2);
#endif
    CHECK(uni_hal_rcc_clksrc_get(UNI_HAL_CORE_PERIPH_ADC_3) == UNI_HAL_RCC_CLKSRC_UNKNOWN);
    return 0U;
}

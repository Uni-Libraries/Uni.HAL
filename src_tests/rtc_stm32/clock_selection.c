#include "clock_model.h"
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_rcc.h>
#else
#include <stm32l4xx_ll_rcc.h>
#endif

static void select_rtc_clock(uint32_t source) {
    // RTCSEL ignores writes once a source is selected, except after LSE failure.
    if (LL_PWR_IsEnabledBkUpAccess() &&
        (LL_RCC_GetRTCClockSource() == LL_RCC_RTC_CLKSOURCE_NONE || (rcc.BDCR & RCC_BDCR_LSECSSD))) {
        LL_RCC_SetRTCClockSource(source);
    }
}
#define LL_RCC_SetRTCClockSource select_rtc_clock
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "rcc/uni_hal_rcc_stm32h7.c"
#else
#include "rcc/uni_hal_rcc_stm32l4.c"
#endif
#include "../arm/semihost_test.h"

static unsigned check(void) {
    g_uni_hal_rcc_status.inited = true;
    uni_hal_core_periph_e const rtc = UNI_HAL_CORE_PERIPH_RTC;
    rcc.BDCR = LL_RCC_RTC_CLKSOURCE_LSE | RCC_BDCR_RTCEN;
    CHECK(!uni_hal_rcc_clksrc_set(rtc, UNI_HAL_RCC_CLKSRC_LSI));
    CHECK(rcc.BDCR == (LL_RCC_RTC_CLKSOURCE_LSE | RCC_BDCR_RTCEN));
    CHECK(!uni_hal_rcc_clksrc_set(rtc, UNI_HAL_RCC_CLKSRC_NONE));
    CHECK(uni_hal_rcc_clksrc_set(rtc, UNI_HAL_RCC_CLKSRC_LSE));
    CHECK(!LL_PWR_IsEnabledBkUpAccess());
    rcc.BDCR |= RCC_BDCR_LSECSSD;
    CHECK(uni_hal_rcc_clksrc_set(rtc, UNI_HAL_RCC_CLKSRC_LSI));
    CHECK(uni_hal_rcc_clksrc_get(rtc) == UNI_HAL_RCC_CLKSRC_LSI);
    for (unsigned open = 0; open <= 1; ++open) {
        rcc.BDCR = 0U;
        uni_hal_pwr_stm_set_backup_access(open != 0U);
        CHECK(!uni_hal_rcc_clksrc_set(rtc, UNI_HAL_RCC_CLKSRC_UNKNOWN));
        CHECK(uni_hal_rcc_clksrc_set(rtc, UNI_HAL_RCC_CLKSRC_NONE));
        CHECK(uni_hal_rcc_clksrc_set(rtc, UNI_HAL_RCC_CLKSRC_LSE));
        CHECK(LL_RCC_GetRTCClockSource() == LL_RCC_RTC_CLKSOURCE_LSE);
        CHECK(!uni_hal_rcc_clksrc_set(rtc, UNI_HAL_RCC_CLKSRC_LSI));
        CHECK(LL_PWR_IsEnabledBkUpAccess() == open);
    }
    rcc.BDCR = 0U;
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    uni_hal_rcc_clksrc_e const hse = UNI_HAL_RCC_CLKSRC_HSE;
#else
    uni_hal_rcc_clksrc_e const hse = UNI_HAL_RCC_CLKSRC_HSE_DIV_32;
#endif
    CHECK(uni_hal_rcc_clksrc_set(rtc, hse));
    CHECK(uni_hal_rcc_clksrc_get(rtc) == hse);
    return 0;
}

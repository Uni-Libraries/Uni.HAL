#include "clock_model.h"
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_rcc.h>
#else
#include <stm32l4xx_ll_rcc.h>
#endif

static bool allow_write = true;
static void write_gate(bool enabled) {
    if (allow_write && LL_PWR_IsEnabledBkUpAccess()) {
        if (enabled) {
            LL_RCC_EnableRTC();
        } else {
            LL_RCC_DisableRTC();
        }
    }
}
#define LL_RCC_EnableRTC() write_gate(true)
#define LL_RCC_DisableRTC() write_gate(false)
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "rcc/uni_hal_rcc_stm32h7.c"
#else
#include "rcc/uni_hal_rcc_stm32l4.c"
#endif
#include "../arm/semihost_test.h"

static unsigned check(void) {
    g_uni_hal_rcc_status.inited = true;
    uni_hal_core_periph_e const rtc = UNI_HAL_CORE_PERIPH_RTC;
    rcc.BDCR = LL_RCC_RTC_CLKSOURCE_LSE;
    for (unsigned i = 0; i < 2; ++i) {
        bool const open = (i == 0);
        uni_hal_pwr_stm_set_backup_access(open);
        CHECK(uni_hal_rcc_clk_set(rtc, true));
        CHECK(LL_PWR_IsEnabledBkUpAccess() == open);
        CHECK(rcc.BDCR == (LL_RCC_RTC_CLKSOURCE_LSE | RCC_BDCR_RTCEN));
        CHECK(uni_hal_rcc_clk_set(rtc, true));
        CHECK(uni_hal_rcc_clk_set(rtc, false));
        CHECK(LL_PWR_IsEnabledBkUpAccess() == open);
        CHECK(rcc.BDCR == LL_RCC_RTC_CLKSOURCE_LSE);
        CHECK(uni_hal_rcc_clk_set(rtc, false));

        allow_write = false;
        CHECK(!uni_hal_rcc_clk_set(rtc, true));
        CHECK(LL_PWR_IsEnabledBkUpAccess() == open);
        CHECK(rcc.BDCR == LL_RCC_RTC_CLKSOURCE_LSE);
        allow_write = true;
    }
    return 0;
}

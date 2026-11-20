#include "clock_model.h"
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "rcc/uni_hal_rcc_stm32h7.c"
#else
#include "rcc/uni_hal_rcc_stm32l4.c"
#endif
#include "../arm/semihost_test.h"

static unsigned check(void) {
    g_uni_hal_rcc_status.inited = true;
    rcc.BDCR = LL_RCC_RTC_CLKSOURCE_LSE | RCC_BDCR_RTCEN;
    for (unsigned open = 0; open <= 1; ++open) {
        uni_hal_pwr_stm_set_backup_access(open != 0U);
        CHECK(uni_hal_rcc_clksrc_get(UNI_HAL_CORE_PERIPH_RTC) == UNI_HAL_RCC_CLKSRC_LSE);
        CHECK(LL_PWR_IsEnabledBkUpAccess() == open);
    }
    return 0;
}

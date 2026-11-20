#include <stm32h7xx.h>
#include <stm32h7xx_ll_rcc.h>
#include "model.h"

static unsigned check_failed_switch(uint32_t source, uint32_t hz) {
    model_pll_switches = false;
    CHECK(!uni_hal_rcc_init());
    uni_hal_rcc_stm32h7_status_t status = uni_hal_rcc_stm32h7_status_get();
    CHECK(!status.inited && !status.sys_inited && status.pll_inited[0]);
    CHECK(SystemCoreClock == hz && model_systick.LOAD + 1U == hz / 1000U);

    // The clock mux processes its outstanding SW request after the caller returns.
    model_complete_clock_switch();
    CHECK((model_rcc.CFGR & RCC_CFGR_SWS) == source << RCC_CFGR_SWS_Pos);
    CHECK(model_requested_clock() == source);
    LL_RCC_ClocksTypeDef clocks;
    LL_RCC_GetSystemClocksFreq(&clocks);
    CHECK(clocks.SYSCLK_Frequency == SystemCoreClock && model_systick.LOAD + 1U == SystemCoreClock / 1000U);
    return 0U;
}

unsigned model_test(void) {
    uni_hal_rcc_stm32h7_config_t config = model_config();
    // A bootloader may have selected CSI before entering the application.
    model_rcc.CR |= RCC_CR_CSION | RCC_CR_CSIRDY;
    model_rcc.CFGR = LL_RCC_SYS_CLKSOURCE_CSI | LL_RCC_SYS_CLKSOURCE_STATUS_CSI;
    config.csi_enable = true;
    model_ms_step = 1U;
    CHECK(uni_hal_rcc_stm32h7_config_set(&config));
    unsigned line = check_failed_switch(LL_RCC_SYS_CLKSOURCE_CSI, CSI_VALUE);
    if (line) return line;

    model_pll_switches = true;
    CHECK(uni_hal_rcc_init() && SystemCoreClock == 100000000U);
    // CSS falls back to HSI when the PLL cannot lock. An unsuccessful ordinary
    // retry must keep that clock stable, and a later successful retry can use PLL.
    model_pll_locks = false;
    model_css();
    CHECK(!uni_hal_rcc_is_inited() && SystemCoreClock == HSI_VALUE);
    model_pll_locks = true;
    line = check_failed_switch(LL_RCC_SYS_CLKSOURCE_HSI, HSI_VALUE);
    if (line) return line;

    model_pll_switches = true;
    CHECK(uni_hal_rcc_init() && SystemCoreClock == 100000000U);
    CHECK((model_rcc.CFGR & RCC_CFGR_SWS) == LL_RCC_SYS_CLKSOURCE_STATUS_PLL1);
    CHECK(model_systick.LOAD + 1U == SystemCoreClock / 1000U);
    // A late PLL acknowledgement can race the attempt to cancel SW. If the
    // mux never acknowledges switching back, freeze the source it actually uses.
    model_pll_locks = false;
    model_css();
    model_pll_locks = true;
    model_pll_switches = false;
    model_late_pll_switch = true;
    CHECK(!uni_hal_rcc_init());
    CHECK(!model_late_pll_switch && model_requested_clock() == LL_RCC_SYS_CLKSOURCE_PLL1);
    model_complete_clock_switch();
    CHECK((model_rcc.CFGR & RCC_CFGR_SWS) == LL_RCC_SYS_CLKSOURCE_STATUS_PLL1);
    CHECK(SystemCoreClock == 100000000U && model_systick.LOAD + 1U == SystemCoreClock / 1000U);
    return 0U;
}

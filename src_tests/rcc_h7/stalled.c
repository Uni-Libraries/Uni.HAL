#include <stm32h7xx.h>
#include <stm32h7xx_ll_rcc.h>
#include "model.h"

unsigned model_test(void) {
    // CSS may already be enabled by a bootloader before the application provides its config.
    model_css_after_claim = true;
    CHECK(!uni_hal_rcc_init());
    CHECK(uni_hal_rcc_stm32h7_status_get().css_fault && !model_hsi_enabled());
    uni_hal_rcc_stm32h7_config_t config = model_config();
    CHECK(uni_hal_rcc_stm32h7_config_set(&config) && uni_hal_rcc_init());
    model_pll_locks = false; // Neither timeout source advances in this NMI.
    model_css();
    CHECK(!uni_hal_rcc_is_inited() && uni_hal_rcc_stm32h7_status_get().sys_inited);
    CHECK(SystemCoreClock == HSI_VALUE && model_clock_reads == 0U);
    return 0;
}

#include <stm32h7xx.h>
#include "model.h"

unsigned model_test(void) {
    uni_hal_rcc_stm32h7_config_t cfg = model_config();
    CHECK(uni_hal_rcc_stm32h7_config_set(&cfg));
    cfg.pll[0].n = 80U;
    CHECK(!uni_hal_rcc_stm32h7_config_set(nullptr));
    CHECK(uni_hal_rcc_init());
    CHECK(SystemCoreClock == 100000000U);

    model_css();
    CHECK(cfg.hse_enable);
    CHECK(cfg.pll[0].n == 80U);
    CHECK(SystemCoreClock == 100000000U);
    CHECK(uni_hal_rcc_stm32h7_status_get().css_fault);
    return 0;
}

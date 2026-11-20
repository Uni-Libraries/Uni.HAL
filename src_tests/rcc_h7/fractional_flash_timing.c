#include <stm32h7xx.h>
#include "model.h"

unsigned model_test(void) {
    // HSE26 / 11 * (118 + 3781/8192) / 2 / 2 = 70,000,005.549 Hz AXI.
    uni_hal_rcc_stm32h7_config_t config = model_config();
    config.pll[0].m = 11U;
    config.pll[0].n = 118U;
    config.pll[0].fracn = 3781U;
    CHECK(uni_hal_rcc_stm32h7_config_set(&config));
    CHECK(uni_hal_rcc_init());
    CHECK(model_flash_hz == 70000006U);

    model_css(); // This PLL reference cannot be reproduced from HSI64.
    CHECK(!uni_hal_rcc_is_inited());

    // This 2 MHz reference can be restored as HSE26/13 or HSI64/32.
    config.pll[0].m = 13U;
    config.pll[0].n = 100U;
    config.pll[0].fracn = 1U;
    CHECK(uni_hal_rcc_stm32h7_config_set(&config));
    CHECK(uni_hal_rcc_init() && model_flash_hz == 50000062U);
    model_css();
    CHECK(uni_hal_rcc_is_inited() && !uni_hal_rcc_stm32h7_status_get().hse_inited);
    return 0U;
}

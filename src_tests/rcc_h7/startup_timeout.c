#include <stm32h7xx.h>
#include <stm32h7xx_ll_rcc.h>
#include "model.h"

unsigned model_test(void) {
    uni_hal_rcc_stm32h7_config_t config = model_config();
    CHECK(uni_hal_rcc_stm32h7_config_set(&config));

    // No DWT and no SysTick interrupt during startup. The model eventually locks
    // so a driver which missed the deadline still returns to the assertion.
    model_pll_delay = 200000U;
    CHECK(!uni_hal_rcc_init());
    CHECK(model_pll_delay != 0U && !uni_hal_rcc_stm32h7_status_get().pll_inited[0]);

    // A frozen DWT must not hide the running millisecond source.
    model_have_dwt = true;
    model_ms_step = 1U;
    model_pll_delay = 32U;
    CHECK(!uni_hal_rcc_init());
    CHECK(model_pll_delay != 0U && !uni_hal_rcc_is_inited());

    // With SysTick masked, use CYCCNT even when the full timeout spans a wrap.
    config.timeout.pll = 100000U; // 6.4 billion cycles at the boot HSI frequency.
    CHECK(uni_hal_rcc_stm32h7_config_set(&config));
    model_ms_step = 0U;
    model_dwt_step = 1000000000U;
    model_pll_delay = 32U;
    CHECK(!uni_hal_rcc_init());
    CHECK(model_pll_delay < 32U - 5U && model_pll_delay != 0U); // Wait past the first CYCCNT wrap.
    CHECK(SystemCoreClock == HSI_VALUE);

    // A healthy oscillator can take several polls while both clocks are stopped.
    config.timeout.pll = 2U;
    CHECK(uni_hal_rcc_stm32h7_config_set(&config));
    model_dwt_step = 0U;
    model_pll_delay = 10U;
    CHECK(uni_hal_rcc_init());
    CHECK(uni_hal_rcc_stm32h7_status_get().pll_inited[0] && SystemCoreClock == 100000000U);
    return 0U;
}

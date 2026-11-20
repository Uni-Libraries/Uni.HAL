#include <stm32h7xx.h>
#include "model.h"

unsigned model_test(void) {
    uni_hal_rcc_stm32h7_config_t cfg = model_config();
    // Drive SYSCLK from PLL1P, FDCAN from PLL2Q and enable only R on PLL3.
    cfg.pll[1] = (uni_hal_rcc_stm32h7_config_pll_t){.m = cfg.pll[0].m, .n = 100U, .q = 4U};
    cfg.pll[2] = (uni_hal_rcc_stm32h7_config_pll_t){.m = cfg.pll[0].m, .n = 120U, .r = 6U};
    CHECK(uni_hal_rcc_stm32h7_config_set(&cfg) && uni_hal_rcc_init());
    CHECK(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_SYSCLK) == 100000000U);
    CHECK(uni_hal_rcc_clksrc_set(UNI_HAL_CORE_PERIPH_CAN_1, UNI_HAL_RCC_CLKSRC_PLL2Q));
    CHECK(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_CAN_1) == 50000000U);
    CHECK(uni_hal_rcc_clksrc_set(UNI_HAL_CORE_PERIPH_CAN_1, UNI_HAL_RCC_CLKSRC_PLL1Q));
    CHECK(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_CAN_1) == 0U);
    CHECK(uni_hal_rcc_clksrc_set(UNI_HAL_CORE_PERIPH_SPI_4, UNI_HAL_RCC_CLKSRC_PLL3Q));
    CHECK(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_SPI_4) == 0U);
    uint32_t const outputs = RCC_PLLCFGR_DIVP1EN | RCC_PLLCFGR_DIVQ1EN | RCC_PLLCFGR_DIVR1EN |
                             RCC_PLLCFGR_DIVP2EN | RCC_PLLCFGR_DIVQ2EN | RCC_PLLCFGR_DIVR2EN |
                             RCC_PLLCFGR_DIVP3EN | RCC_PLLCFGR_DIVQ3EN | RCC_PLLCFGR_DIVR3EN;
    CHECK((model_rcc.PLLCFGR & outputs) == (RCC_PLLCFGR_DIVP1EN | RCC_PLLCFGR_DIVQ2EN | RCC_PLLCFGR_DIVR3EN));

    // Retry after a clock fault with PLL1Q serving FDCAN and no PLL1P for SYSCLK.
    model_pll_locks = false;
    model_css();
    CHECK(!uni_hal_rcc_is_inited());
    model_pll_locks = true;
    cfg.pll[0].p = 0U;
    cfg.pll[0].q = 8U;
    cfg.pll[1] = (uni_hal_rcc_stm32h7_config_pll_t){0};
    cfg.pll[2] = (uni_hal_rcc_stm32h7_config_pll_t){0};
    CHECK(uni_hal_rcc_stm32h7_config_set(&cfg) && uni_hal_rcc_init());
    CHECK((model_rcc.PLLCFGR & outputs) == RCC_PLLCFGR_DIVQ1EN);
    CHECK(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_SYSCLK) == HSE_VALUE);
    CHECK(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_CAN_1) == 25000000U);
    model_pll_locks = false;
    model_css();
    CHECK(!uni_hal_rcc_is_inited());
    cfg.pll[0] = (uni_hal_rcc_stm32h7_config_pll_t){0};
    CHECK(uni_hal_rcc_stm32h7_config_set(&cfg) && uni_hal_rcc_init());
    CHECK((model_rcc.PLLCFGR & outputs) == 0U && !(model_rcc.CR & RCC_CR_PLL1ON));
    CHECK(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_SYSCLK) == HSE_VALUE);
    return 0U;
}

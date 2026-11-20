#include <stm32h7xx.h>
#include "model.h"

unsigned model_test(void) {
    uni_hal_rcc_stm32h7_config_t cfg = model_config();
    cfg.pll[0].fracn = 8192U;
    cfg.pll[1] = (uni_hal_rcc_stm32h7_config_pll_t){.m = cfg.pll[0].m, .n = 80U, .q = 4U, .fracn = 8192U};
    cfg.pll[2] = (uni_hal_rcc_stm32h7_config_pll_t){.m = cfg.pll[0].m, .n = 90U, .q = 4U, .fracn = 8192U};
    CHECK(uni_hal_rcc_stm32h7_config_set(&cfg) && uni_hal_rcc_init());
    CHECK(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_SYSCLK) == 101000000U);
    CHECK(uni_hal_rcc_clksrc_set(UNI_HAL_CORE_PERIPH_CAN_1, UNI_HAL_RCC_CLKSRC_PLL2Q));
    CHECK(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_CAN_1) == 40500000U);
    CHECK(uni_hal_rcc_clksrc_set(UNI_HAL_CORE_PERIPH_SPI_4, UNI_HAL_RCC_CLKSRC_PLL3Q));
    CHECK(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_SPI_4) == 45500000U);
    CHECK(!model_rcc.PLL1FRACR && !model_rcc.PLL2FRACR && !model_rcc.PLL3FRACR);
    CHECK(!(model_rcc.PLLCFGR & (RCC_PLLCFGR_PLL1FRACEN | RCC_PLLCFGR_PLL2FRACEN | RCC_PLLCFGR_PLL3FRACEN)));

    model_pll_locks = false;
    model_css();
    CHECK(!uni_hal_rcc_is_inited());
    model_pll_locks = true;
    cfg.pll[0] = (uni_hal_rcc_stm32h7_config_pll_t){.m = HSE_VALUE / 1000000U, .n = 511U, .p = 4U, .fracn = 8192U};
    cfg.pll[1].q = 0U;
    cfg.pll[1].fracn = UINT32_MAX; // Unused PLL settings do not affect startup.
    cfg.pll[2].fracn = 8191U;
    CHECK(uni_hal_rcc_stm32h7_config_set(&cfg));
    CHECK(cfg.pll[0].n == 511U && cfg.pll[0].fracn == 8192U);

    // Rejected configurations must leave the last accepted snapshot available for retry.
    cfg.pll[0].n = 512U;
    CHECK(!uni_hal_rcc_stm32h7_config_set(&cfg));
    cfg.pll[0].n = 511U;
    cfg.pll[0].fracn = 8193U;
    CHECK(!uni_hal_rcc_stm32h7_config_set(&cfg));
    cfg.pll[0].fracn = 0U;
    cfg.pll[1].q = 2U;
    CHECK(!uni_hal_rcc_stm32h7_config_set(&cfg));
    CHECK(uni_hal_rcc_init());
    CHECK(uni_hal_rcc_clk_get_freq(UNI_HAL_CORE_PERIPH_SYSCLK) == 128000000U);
    CHECK(!(model_rcc.CR & RCC_CR_PLL2ON));
    CHECK((model_rcc.PLL3FRACR & RCC_PLL3FRACR_FRACN3) == (8191U << RCC_PLL3FRACR_FRACN3_Pos));
    return 0U;
}

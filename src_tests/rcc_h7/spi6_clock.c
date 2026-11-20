#include <stm32h7xx.h>
#include "model.h"

unsigned model_test(void) {
    static const uni_hal_rcc_clksrc_e sources[] = {
        UNI_HAL_RCC_CLKSRC_PCLK4, UNI_HAL_RCC_CLKSRC_PLL2Q, UNI_HAL_RCC_CLKSRC_PLL3Q,
        UNI_HAL_RCC_CLKSRC_HSI, UNI_HAL_RCC_CLKSRC_CSI, UNI_HAL_RCC_CLKSRC_HSE,
    };
    uni_hal_rcc_stm32h7_config_t cfg = model_config();
    CHECK(uni_hal_rcc_stm32h7_config_set(&cfg));
    CHECK(uni_hal_rcc_init());

    CHECK(uni_hal_rcc_clksrc_set(UNI_HAL_CORE_PERIPH_SPI_5, UNI_HAL_RCC_CLKSRC_HSE));
    uint32_t const spi45_reg = model_rcc.D2CCIP1R;
    uint32_t const other_bits = RCC_D3CCIPR_I2C4SEL_0;
    for (unsigned i = 0; i < sizeof sources / sizeof sources[0]; ++i) {
        uint32_t const selection = other_bits | (i << RCC_D3CCIPR_SPI6SEL_Pos);
        model_rcc.D3CCIPR = selection;
        CHECK(uni_hal_rcc_clksrc_get(UNI_HAL_CORE_PERIPH_SPI_6) == sources[i]);

        model_rcc.D3CCIPR = other_bits | RCC_D3CCIPR_SPI6SEL;
        CHECK(uni_hal_rcc_clksrc_set(UNI_HAL_CORE_PERIPH_SPI_6, sources[i]));
        CHECK(model_rcc.D3CCIPR == selection);
        CHECK(model_rcc.D2CCIP1R == spi45_reg);
    }

    for (unsigned i = 6; i <= 7; ++i) {
        uint32_t const reserved = other_bits | (i << RCC_D3CCIPR_SPI6SEL_Pos);
        model_rcc.D3CCIPR = reserved;
        CHECK(uni_hal_rcc_clksrc_get(UNI_HAL_CORE_PERIPH_SPI_6) == UNI_HAL_RCC_CLKSRC_UNKNOWN);
        CHECK(!uni_hal_rcc_clksrc_set(UNI_HAL_CORE_PERIPH_SPI_6, UNI_HAL_RCC_CLKSRC_PCLK2));
        CHECK(model_rcc.D3CCIPR == reserved);
    }
    return 0;
}

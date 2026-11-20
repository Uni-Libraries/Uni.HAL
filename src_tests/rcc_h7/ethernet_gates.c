#include <stm32h7xx.h>
#include "model.h"

unsigned model_test(void) {
    uint32_t const gates = RCC_AHB1ENR_ETH1MACEN | RCC_AHB1ENR_ETH1RXEN | RCC_AHB1ENR_ETH1TXEN;
    uint32_t const neighbor = RCC_AHB1ENR_DMA1EN;
    uni_hal_core_periph_e const eth = UNI_HAL_CORE_PERIPH_ETH_1;

    for (unsigned subset = 0; subset < 8; ++subset) {
        uint32_t const initial = neighbor
            | ((subset & 1U) ? RCC_AHB1ENR_ETH1MACEN : 0U)
            | ((subset & 2U) ? RCC_AHB1ENR_ETH1RXEN : 0U)
            | ((subset & 4U) ? RCC_AHB1ENR_ETH1TXEN : 0U);
        model_rcc.AHB1ENR = initial;
        CHECK(uni_hal_rcc_clk_get(eth) == ((initial & gates) == gates));
        CHECK(uni_hal_rcc_clk_set(eth, true));
        CHECK(model_rcc.AHB1ENR == (neighbor | gates));
        CHECK(uni_hal_rcc_clk_get(eth));
        model_rcc.AHB1ENR = initial;
        CHECK(uni_hal_rcc_clk_set(eth, false));
        CHECK(model_rcc.AHB1ENR == neighbor);
        CHECK(!uni_hal_rcc_clk_get(eth));
    }
    return 0;
}

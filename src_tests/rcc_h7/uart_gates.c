#include <stm32h7xx.h>
#include "model.h"

unsigned model_test(void) {
    struct {
        uni_hal_core_periph_e uart;
        volatile uint32_t *reg;
        uint32_t gate;
    } const ports[] = {
        {UNI_HAL_CORE_PERIPH_UART_6, &model_rcc.APB2ENR, RCC_APB2ENR_USART6EN},
        {UNI_HAL_CORE_PERIPH_UART_7, &model_rcc.APB1LENR, RCC_APB1LENR_UART7EN},
        {UNI_HAL_CORE_PERIPH_UART_8, &model_rcc.APB1LENR, RCC_APB1LENR_UART8EN},
    };
    model_rcc.APB2ENR = RCC_APB2ENR_USART1EN;
    model_rcc.APB1LENR = RCC_APB1LENR_UART5EN;
    for (size_t i = 0; i < sizeof ports / sizeof ports[0]; ++i) {
        uint32_t const initial = *ports[i].reg;
        CHECK(!uni_hal_rcc_clk_get(ports[i].uart));
        CHECK(uni_hal_rcc_clk_set(ports[i].uart, true));
        CHECK(*ports[i].reg == (initial | ports[i].gate));
        CHECK(uni_hal_rcc_clk_get(ports[i].uart));
        CHECK(uni_hal_rcc_clk_set(ports[i].uart, false));
        CHECK(*ports[i].reg == initial);
        CHECK(!uni_hal_rcc_clk_get(ports[i].uart));
    }
    return 0;
}

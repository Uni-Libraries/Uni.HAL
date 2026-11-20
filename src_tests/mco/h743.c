#include <stm32h7xx.h>

static RCC_TypeDef rcc;
#undef RCC
#define RCC (&rcc)
#include "rcc/uni_hal_rcc_stm32h7.c"
#include "rcc/uni_hal_rcc_stm32.h"
#include "../arm/semihost_test.h"
#include "gpio_fixture.h"

static unsigned check(void) {
    const uint32_t initial = RCC_CFGR_TIMPRE | RCC_CFGR_MCO1PRE_0 | RCC_CFGR_MCO2PRE_0;
    rcc.CFGR = initial;
    CHECK(!uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_SYSCLK, 1));
    CHECK(!uni_hal_rcc_stm32_mco_enable(1, UNI_HAL_RCC_CLKSRC_HSI, 1));
    CHECK(!uni_hal_rcc_stm32_mco_enable(2, UNI_HAL_RCC_CLKSRC_HSE, 1));
    CHECK(!uni_hal_rcc_stm32_mco_enable(1, UNI_HAL_RCC_CLKSRC_HSE, 16));
    CHECK(gpio_calls == 0 && rcc.CFGR == initial);

    gpio_ok = false;
    CHECK(!uni_hal_rcc_stm32_mco_enable(1, UNI_HAL_RCC_CLKSRC_HSE, 5));
    CHECK(gpio_calls == 1 && rcc.CFGR == initial);

    gpio_ok = true;
    CHECK(uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_HSE, 15));
    CHECK(gpio_calls == 2 && gpio_pin.gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_A);
    CHECK(gpio_pin.gpio_pin == UNI_HAL_GPIO_PIN_8 && gpio_pin.alternate == UNI_HAL_GPIO_ALTERNATE_0);
    const uint32_t mco1 = RCC_CFGR_MCO1_1 | (15U << RCC_CFGR_MCO1PRE_Pos);
    CHECK(rcc.CFGR == (RCC_CFGR_TIMPRE | mco1 | RCC_CFGR_MCO2PRE_0));

    CHECK(uni_hal_rcc_stm32_mco_enable(1, UNI_HAL_RCC_CLKSRC_PLL2P, 7));
    CHECK(gpio_calls == 3 && gpio_pin.gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_C);
    CHECK(gpio_pin.gpio_pin == UNI_HAL_GPIO_PIN_9 && gpio_pin.alternate == UNI_HAL_GPIO_ALTERNATE_0);
    const uint32_t mco2 = RCC_CFGR_MCO2_0 | (7U << RCC_CFGR_MCO2PRE_Pos);
    CHECK(rcc.CFGR == (RCC_CFGR_TIMPRE | mco1 | mco2));
    return 0;
}

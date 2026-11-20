#include <stm32l4xx.h>

static RCC_TypeDef rcc;
#undef RCC
#define RCC (&rcc)
#include "rcc/uni_hal_rcc_stm32l4.c"
#include "rcc/uni_hal_rcc_stm32.h"
#include "../arm/semihost_test.h"

#include "gpio_fixture.h"

static unsigned check(void) {
    const uint32_t initial = RCC_CFGR_STOPWUCK | LL_RCC_MCO1SOURCE_MSI | LL_RCC_MCO1_DIV_2;
    rcc.CFGR = initial;
    CHECK(!uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_UNKNOWN, 1));
    CHECK(!uni_hal_rcc_stm32_mco_enable(1, UNI_HAL_RCC_CLKSRC_HSE, 1));
    CHECK(!uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_HSE, 3));
    CHECK(gpio_calls == 0 && rcc.CFGR == initial);

    gpio_ok = false;
    CHECK(!uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_HSI, 1));
    CHECK(gpio_calls == 1 && rcc.CFGR == initial);

    gpio_ok = true;
    CHECK(uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_HSE, 16));
    CHECK(gpio_calls == 2 && gpio_pin.gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_A);
    CHECK(gpio_pin.gpio_pin == UNI_HAL_GPIO_PIN_8 && gpio_pin.alternate == UNI_HAL_GPIO_ALTERNATE_0);
    CHECK(gpio_pin.gpio_type == UNI_HAL_GPIO_TYPE_ALTERNATE_PP);
    CHECK(rcc.CFGR == (RCC_CFGR_STOPWUCK | LL_RCC_MCO1SOURCE_HSE | LL_RCC_MCO1_DIV_16));
    return 0;
}

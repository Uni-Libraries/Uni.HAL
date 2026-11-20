// Exercise both shared H743 SPI kernel muxes through the public driver and STM32 LL.
#include <stm32h7xx.h>

static RCC_TypeDef rcc;
static SPI_TypeDef ports[6];
#undef RCC_BASE
#define RCC_BASE ((uintptr_t)&rcc)
#undef SPI1
#undef SPI2
#undef SPI3
#undef SPI4
#undef SPI5
#undef SPI6
#define SPI1 (&ports[0])
#define SPI2 (&ports[1])
#define SPI3 (&ports[2])
#define SPI4 (&ports[3])
#define SPI5 (&ports[4])
#define SPI6 (&ports[5])

#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_rcc.c"
#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_spi.c"
#include "rcc/uni_hal_rcc_stm32h7.c"
#include "spi/uni_hal_spi.h"

static uni_hal_spi_context_t *contender;
static bool contender_result;
static bool init_pin(uni_hal_gpio_pin_context_t *pin) {
    if (contender) {
        uni_hal_spi_context_t *other = contender;
        contender = nullptr;
        contender_result = uni_hal_spi_init(other);
    }
    pin->inited = true;
    return true;
}

#define uni_hal_gpio_pin_init init_pin
#include "spi/uni_hal_spi_stm32h7.c"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    g_uni_hal_rcc_status.inited = true;
    rcc.CR = RCC_CR_CSIRDY | RCC_CR_HSIRDY;

    uni_hal_gpio_pin_context_t pins[6] = {};
    uni_hal_spi_context_t ctx[6] = {};
    for (unsigned i = 0; i < 6U; ++i) {
        ctx[i].config.instance = (uni_hal_core_periph_e)(UNI_HAL_CORE_PERIPH_SPI_1 + i);
        ctx[i].config.mode = UNI_HAL_SPI_MODE_MASTER;
        ctx[i].config.pin_sck = &pins[i];
        ctx[i].config.prescaler = UNI_HAL_SPI_PRESCALER_4;
    }

    // SPI5 interrupts SPI4 initialization, before GPIO and peripheral setup have finished.
    ctx[3].config.clock_source = UNI_HAL_RCC_CLKSRC_HSI;
    ctx[4].config.clock_source = UNI_HAL_RCC_CLKSRC_CSI;
    contender = &ctx[4];
    CHECK(uni_hal_spi_init(&ctx[3]));
    CHECK(!contender_result && !pins[4].inited && !LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_SPI5));
    CHECK(uni_hal_spi_bitrate_get(&ctx[3]) == HSI_VALUE / 4U);
    CHECK(!uni_hal_spi_init(&ctx[4]) && !uni_hal_spi_is_inited(&ctx[4]));
    ctx[4].config.clock_source = UNI_HAL_RCC_CLKSRC_HSI;
    CHECK(uni_hal_spi_init(&ctx[4]) && uni_hal_spi_bitrate_get(&ctx[4]) == HSI_VALUE / 4U);

    // A conflicting replacement of an idle port must preserve its original owner and sibling.
    uni_hal_spi_context_t replacement = ctx[3];
    replacement.config.clock_source = UNI_HAL_RCC_CLKSRC_CSI;
    CHECK(!uni_hal_spi_init(&replacement));
    CHECK(uni_hal_spi_is_inited(&ctx[3]) && uni_hal_spi_bitrate_get(&ctx[4]) == HSI_VALUE / 4U);

    // An idle port can still change source on replacement if it is the group's only owner.
    uni_hal_spi_context_t previous = ctx[2];
    previous.config.clock_source = UNI_HAL_RCC_CLKSRC_PLL2P;
    CHECK(uni_hal_spi_init(&previous));
    ctx[2].config.clock_source = UNI_HAL_RCC_CLKSRC_CLKP;
    CHECK(uni_hal_spi_init(&ctx[2]) && !uni_hal_spi_is_inited(&previous));
    ctx[0].config.clock_source = UNI_HAL_RCC_CLKSRC_PLL2P;
    CHECK(!uni_hal_spi_init(&ctx[0]) && !pins[0].inited);
    CHECK(LL_RCC_GetSPIClockSource(LL_RCC_SPI123_CLKSOURCE) == LL_RCC_SPI123_CLKSOURCE_CLKP);
    ctx[1].config.clock_source = UNI_HAL_RCC_CLKSRC_CLKP;
    CHECK(uni_hal_spi_init(&ctx[1]));
    ctx[5].config.clock_source = UNI_HAL_RCC_CLKSRC_CSI;
    CHECK(uni_hal_spi_init(&ctx[5]) && uni_hal_spi_bitrate_get(&ctx[5]) == CSI_VALUE / 4U);
    CHECK(uni_hal_spi_bitrate_get(&ctx[3]) == HSI_VALUE / 4U);
    return 0;
}

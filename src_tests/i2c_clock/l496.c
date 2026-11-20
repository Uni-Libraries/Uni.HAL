// Exercise the public I2C init, RCC mux and vendor TIMINGR write on registers in RAM.
#include <stm32l4xx.h>

static RCC_TypeDef rcc;
static I2C_TypeDef ports[4];
#undef RCC_BASE
#define RCC_BASE ((uintptr_t)&rcc)
#undef I2C1
#undef I2C2
#undef I2C3
#undef I2C4
#define I2C1 (&ports[0])
#define I2C2 (&ports[1])
#define I2C3 (&ports[2])
#define I2C4 (&ports[3])

#include "../../3rdparty/st_stm32l4/src_hal/stm32l4xx_ll_rcc.c"
#include "rcc/uni_hal_rcc_stm32l4.c"
#include "i2c/uni_hal_i2c_stm32l4.c"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    g_uni_hal_rcc_status.inited = true;
    rcc.CR = RCC_CR_HSIRDY | RCC_CR_HSERDY;
    rcc.CFGR = LL_RCC_SYS_CLKSOURCE_STATUS_HSE | LL_RCC_APB1_DIV_2;
    // I2C2 starts on HSI. All other I2C muxes start on PCLK1.
    rcc.CCIPR = RCC_CCIPR_I2C2SEL_1 | RCC_CCIPR_CLK48SEL_0;
    uni_hal_gpio_pin_context_t scl = {.inited = true}, sda = {.inited = true};
    uni_hal_i2c_context_t ctx[4] = {};
    for (unsigned i = 0; i < 4U; ++i) {
        ctx[i].config.instance = (uni_hal_core_periph_e)(UNI_HAL_CORE_PERIPH_I2C_1 + i);
        ctx[i].config.pin_sck = &scl;
        ctx[i].config.pin_sda = &sda;
    }

    ctx[0].config.clock_source = UNI_HAL_RCC_CLKSRC_SYSCLK;
    CHECK(uni_hal_i2c_init(&ctx[0]));
    CHECK(ports[0].TIMINGR == uni_hal_i2c_timing_calc(HSE_VALUE, UNI_HAL_I2C_SPEED_100KHZ));
    // Zero-initialized config keeps the existing mux selection.
    CHECK(uni_hal_i2c_init(&ctx[1]));
    CHECK(ports[1].TIMINGR == uni_hal_i2c_timing_calc(HSI_VALUE, UNI_HAL_I2C_SPEED_100KHZ));

    ctx[2].config.clock_source = UNI_HAL_RCC_CLKSRC_HSI;
    ctx[2].config.timing = 0x10203040U;
    CHECK(uni_hal_i2c_init(&ctx[2]) && ports[2].TIMINGR == ctx[2].config.timing);
    uint32_t const expected_ccipr = RCC_CCIPR_I2C1SEL_0 | RCC_CCIPR_I2C2SEL_1 |
        RCC_CCIPR_I2C3SEL_1 | RCC_CCIPR_CLK48SEL_0;
    CHECK(rcc.CCIPR == expected_ccipr);

    rcc.CCIPR2 = RCC_CCIPR2_I2C4SEL_1;
    ctx[3].config.clock_source = UNI_HAL_RCC_CLKSRC_LSE;
    CHECK(!uni_hal_i2c_init(&ctx[3]) && !ctx[3].state.initialized);
    CHECK(rcc.CCIPR == expected_ccipr && rcc.CCIPR2 == RCC_CCIPR2_I2C4SEL_1);
    CHECK(ports[3].CR1 == 0);
    ctx[3].config.clock_source = UNI_HAL_RCC_CLKSRC_PCLK1;
    CHECK(uni_hal_i2c_init(&ctx[3]));
    CHECK(rcc.CCIPR == expected_ccipr && rcc.CCIPR2 == 0U);
    CHECK(ports[3].TIMINGR == uni_hal_i2c_timing_calc(HSE_VALUE / 2U, UNI_HAL_I2C_SPEED_100KHZ));
    return 0;
}

// Exercise the shared kernel mux and the actual ST TIMINGR writes on peripherals in RAM.
#include <stm32h7xx.h>

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

#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_rcc.c"
#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_i2c.c"
#include "rcc/uni_hal_rcc_stm32h7.c"
#include "i2c/uni_hal_i2c.h"

static uni_hal_i2c_context_t *contender;
static bool contender_result;
static bool init_pin(uni_hal_gpio_pin_context_t *pin) {
    // Simulate preemption by another bus initializer before the first TIMINGR is written.
    if (contender) {
        uni_hal_i2c_context_t *other = contender;
        contender = nullptr;
        contender_result = uni_hal_i2c_init(other);
    }
    pin->inited = true;
    return true;
}
#define uni_hal_gpio_pin_init init_pin
#include "i2c/uni_hal_i2c_stm32h7.c"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    g_uni_hal_rcc_status.inited = true;
    rcc.CR = RCC_CR_HSIRDY | RCC_CR_CSIRDY;
    // A board has already selected CSI for I2C1..3.
    LL_RCC_SetI2CClockSource(LL_RCC_I2C123_CLKSOURCE_CSI);
    uni_hal_gpio_pin_context_t scl[4] = {}, sda[4] = {};
    uni_hal_i2c_context_t ctx[4] = {};
    for (unsigned i = 0; i < 4U; ++i) {
        ctx[i].config.instance = (uni_hal_core_periph_e)(UNI_HAL_CORE_PERIPH_I2C_1 + i);
        ctx[i].config.pin_sck = &scl[i];
        ctx[i].config.pin_sda = &sda[i];
    }

    // An unspecified source adopts the board selection, even during another init.
    contender = &ctx[1];
    ctx[1].config.clock_source = UNI_HAL_RCC_CLKSRC_HSI;
    CHECK(uni_hal_i2c_init(&ctx[0]));
    CHECK(!contender_result && !ctx[1].state.initialized && !scl[1].inited);
    CHECK(LL_RCC_GetI2CClockSource(LL_RCC_I2C123_CLKSOURCE) == LL_RCC_I2C123_CLKSOURCE_CSI);
    CHECK(ports[0].TIMINGR == uni_hal_i2c_timing_calc(CSI_VALUE, UNI_HAL_I2C_SPEED_100KHZ));

    CHECK(!uni_hal_i2c_init(&ctx[1]));
    ctx[1].config.clock_source = UNI_HAL_RCC_CLKSRC_CSI;
    CHECK(uni_hal_i2c_init(&ctx[1]) && ports[1].TIMINGR == ports[0].TIMINGR);
    CHECK(uni_hal_i2c_init(&ctx[2]) && ports[2].TIMINGR == ports[0].TIMINGR);

    ctx[3].config.timing = 0x10203040U;
    ctx[3].config.clock_source = UNI_HAL_RCC_CLKSRC_PCLK1; // I2C4 is on APB4.
    CHECK(!uni_hal_i2c_init(&ctx[3]) && !scl[3].inited && !ports[3].CR1);
    ctx[3].config.clock_source = UNI_HAL_RCC_CLKSRC_HSI;
    CHECK(uni_hal_i2c_init(&ctx[3]) && ports[3].TIMINGR == ctx[3].config.timing);
    CHECK(LL_RCC_GetI2CClockSource(LL_RCC_I2C4_CLKSOURCE) == LL_RCC_I2C4_CLKSOURCE_HSI);
    CHECK(LL_RCC_GetI2CClockSource(LL_RCC_I2C123_CLKSOURCE) == LL_RCC_I2C123_CLKSOURCE_CSI);

    CHECK(uni_hal_i2c_deinit(&ctx[0]) && uni_hal_i2c_deinit(&ctx[1]));
    ctx[0].config.clock_source = UNI_HAL_RCC_CLKSRC_HSI;
    CHECK(!uni_hal_i2c_init(&ctx[0])); // I2C3 still uses CSI.
    CHECK(uni_hal_i2c_deinit(&ctx[2]));
    CHECK(uni_hal_i2c_init(&ctx[0]));
    CHECK(ports[0].TIMINGR == uni_hal_i2c_timing_calc(HSI_VALUE, UNI_HAL_I2C_SPEED_100KHZ));
    // A failed timing calculation must release the claim for other buses.
    CHECK(uni_hal_i2c_deinit(&ctx[0]));
    ctx[0].config.clock_source = UNI_HAL_RCC_CLKSRC_PLL3R;
    CHECK(!uni_hal_i2c_init(&ctx[0])); // PLL3 is off.
    CHECK(uni_hal_i2c_init(&ctx[1]));
    CHECK(ports[1].TIMINGR == uni_hal_i2c_timing_calc(CSI_VALUE, UNI_HAL_I2C_SPEED_100KHZ));
    return 0;
}

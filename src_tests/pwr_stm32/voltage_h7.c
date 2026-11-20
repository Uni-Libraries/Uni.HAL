#include <stm32h7xx.h>

static PWR_TypeDef pwr;
static SYSCFG_TypeDef syscfg;
#undef PWR
#undef SYSCFG
#define PWR (&pwr)
#define SYSCFG (&syscfg)
#include <stm32h7xx_ll_pwr.h>
#include "dwt/uni_hal_dwt.h"

static unsigned polls, ready_at;
static uint32_t ms, ms_step, cycle, cycle_step;
static bool have_dwt;

static uint32_t model_supply_ready(void) {
    if (++polls == ready_at) {
        pwr.CSR1 |= PWR_CSR1_ACTVOSRDY;
        pwr.D3CR |= PWR_D3CR_VOSRDY;
    }
    return LL_PWR_IsActiveFlag_ACTVOS();
}

static uint32_t model_dwt_tick(void) {
    cycle += cycle_step;
    return cycle;
}

#define LL_PWR_IsActiveFlag_ACTVOS model_supply_ready
#define uni_hal_systick_get_ms model_tick
#define uni_hal_rcc_clk_set model_clock_set
#define uni_hal_dwt_is_inited() have_dwt
#define uni_hal_dwt_init() true
#define uni_hal_dwt_get_tick model_dwt_tick
#include "pwr/uni_hal_pwr_stm32h7.c"
#include "../arm/semihost_test.h"

uint32_t model_tick(void) {
    ms += ms_step;
    return ms;
}

bool model_clock_set(uni_hal_core_periph_e target, bool enable) {
    return target == UNI_HAL_CORE_PERIPH_SYSCFG && enable;
}

static unsigned check(void) {
    SystemCoreClock = 100000U;
    // DWT accepts initialization but CYCCNT never advances, and the boot SysTick is stopped.
    ready_at = 50000U;
    CHECK(!uni_hal_pwr_init() && !uni_hal_pwr_is_inited());
    CHECK(polls < ready_at && !syscfg.PWRCR);

    polls = 0;
    have_dwt = true;
    ms = UINT32_MAX - 50U;
    ms_step = 25U;
    CHECK(!uni_hal_pwr_init() && polls < 15U);

    polls = 0;
    ms_step = 0;
    cycle = UINT32_MAX - 8000U;
    cycle_step = 6000U;
    CHECK(!uni_hal_pwr_init() && polls < 10U);

    polls = 0;
    cycle_step = 0;
    ready_at = 12U;
    CHECK(uni_hal_pwr_init() && uni_hal_pwr_is_inited());
    CHECK(LL_PWR_GetRegulVoltageScaling() == LL_PWR_REGU_VOLTAGE_SCALE0);
    CHECK(syscfg.PWRCR & SYSCFG_PWRCR_ODEN);
    return 0;
}

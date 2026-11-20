#include "model_l4.h"

static unsigned check(void) {
    SystemCoreClock = 4000000U;
    pwr.CR1 = LL_PWR_REGU_VOLTAGE_SCALE2;
    pwr.SR2 = PWR_SR2_VOSF;
    CHECK(!uni_hal_pwr_init());
    CHECK(!uni_hal_pwr_is_inited() && LL_PWR_IsActiveFlag_VOS());
    CHECK(LL_PWR_GetRegulVoltageScaling() == LL_PWR_REGU_VOLTAGE_SCALE1);

    // VOS already selects range 1 on a retry, but the regulator still needs to settle.
    ready_at = polls + 12U;
    CHECK(uni_hal_pwr_init() && uni_hal_pwr_is_inited());
    CHECK(!LL_PWR_IsActiveFlag_VOS());

    pwr.SR2 = PWR_SR2_VOSF;
    ms = UINT32_MAX;
    step_ms = 1U;
    polls = 0;
    ready_at = 1000U;
    CHECK(!uni_hal_pwr_init() && !uni_hal_pwr_is_inited());
    CHECK(polls < ready_at);
    return 0;
}

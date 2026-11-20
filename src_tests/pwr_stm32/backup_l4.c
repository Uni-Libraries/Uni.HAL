#include "model_l4.h"

static unsigned check(void) {
    // An RTC owner has already unlocked the domain for calendar writes.
    LL_PWR_EnableBkUpAccess();
    CHECK(uni_hal_pwr_set_battery_charging(true));
    CHECK(uni_hal_pwr_init());
    CHECK(LL_PWR_IsEnabledBkUpAccess() && LL_PWR_IsEnabledBatteryCharging());

    LL_PWR_DisableBkUpAccess();
    CHECK(uni_hal_pwr_set_battery_charging(false));
    CHECK(uni_hal_pwr_init());
    CHECK(!LL_PWR_IsEnabledBkUpAccess() && !LL_PWR_IsEnabledBatteryCharging());
    return 0;
}

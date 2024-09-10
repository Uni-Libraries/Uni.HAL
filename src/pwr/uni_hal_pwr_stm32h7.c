//
// Includes
//

// stdlib
#include <stddef.h>

// ST
#include <stm32h7xx_ll_pwr.h>

// Uni.HAL
#include "pwr/uni_hal_pwr.h"
#include "pwr/uni_hal_pwr_stm32.h"
#include "rcc/uni_hal_rcc.h"


//
// Globals
//

extern uni_hal_pwr_context_t g_uni_hal_pwr_ctx;



//
// Private
//

static void _uni_hal_stm_pwr_voltagescale(void) {
    LL_PWR_ConfigSupply(LL_PWR_LDO_SUPPLY);
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    while (LL_PWR_IsActiveFlag_VOS() == 0) {
    }
}


//
// Functions
//


bool uni_hal_pwr_init(void) {
    bool result = false;
    result = uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_SYSCFG, true);
    result = uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_PWR, true) && result;
    _uni_hal_stm_pwr_voltagescale();
    g_uni_hal_pwr_ctx.inited = true;
    return result;
}


bool uni_hal_pwr_set_battery_charging(bool value) {
    uni_hal_pwr_stm_set_backup_access(true);
    if (value) {
        LL_PWR_SetBattChargResistor(LL_PWR_BATT_CHARGRESISTOR_1_5K);
        LL_PWR_EnableBatteryCharging();
    } else {
        LL_PWR_DisableBatteryCharging();
    }
    uni_hal_pwr_stm_set_backup_access(false);
    g_uni_hal_pwr_ctx.battery_charging = value;

    return true;
}

//
// Includes
//

// stdlib
#include <stddef.h>

// ST
#include <stm32l4xx_ll_pwr.h>

// Uni.HAL
#include "core/uni_hal_core.h"
#include "pwr/uni_hal_pwr.h"
#include "pwr/uni_hal_pwr_stm32.h"
#include "pwr/uni_hal_pwr_stm32l4.h"
#include "rcc/uni_hal_rcc.h"

//
// Globals
//

extern uni_hal_pwr_context_t g_uni_hal_pwr_ctx;


//
// Private
//

static void _uni_hal_stm_pwr_voltagescale(void) { LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1); }


//
// Functions
//


bool uni_hal_pwr_init() {
    bool result = uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_SYSCFG, true);
    result = uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_PWR, true) && result;
    _uni_hal_stm_pwr_voltagescale();

    LL_PWR_EnableBkUpAccess();

    if (g_uni_hal_pwr_ctx.battery_charging) {
        LL_PWR_SetBattChargResistor(LL_PWR_BATT_CHARGRESISTOR_1_5K);
        LL_PWR_EnableBatteryCharging();
    } else {
        LL_PWR_DisableBatteryCharging();
    }

    LL_PWR_DisableBkUpAccess();
    g_uni_hal_pwr_ctx.inited = true;

    return result;
}


//
// STM32
//

void uni_hal_pwr_stm32l4_set_backup_access(bool val) {
    val ? LL_PWR_EnableBkUpAccess() : LL_PWR_DisableBkUpAccess();
}



//
// STM32 L4
//

void uni_hal_pwr_stm_l4_set_vddio2(bool val)
{
    val ? LL_PWR_EnableVddIO2() : LL_PWR_DisableVddIO2();
}

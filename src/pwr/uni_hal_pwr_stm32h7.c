//
// Includes
//

// stdlib
#include <stddef.h>
#include <stdint.h>

// ST
#include <stm32h7xx.h>
#include <stm32h7xx_ll_pwr.h>

// Uni.HAL
#include "dwt/uni_hal_dwt_stm32_wait.h"
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

enum { UNI_HAL_PWR_VOS_TIMEOUT_MS = 200U };

static bool _uni_hal_stm_pwr_is_voltage_ready(void) {
    return (LL_PWR_IsActiveFlag_ACTVOS() != 0U) && (LL_PWR_IsActiveFlag_VOS() != 0U);
}


static bool _uni_hal_stm_pwr_wait_vos_ready(void) {
    if (SystemCoreClock == 0U) {
        return false;
    }

    if (_uni_hal_stm_pwr_is_voltage_ready()) {
        return true;
    }

    if (!uni_hal_dwt_is_inited()) {
        (void)uni_hal_dwt_init();
    }
    uni_hal_stm32_clock_timeout_t timeout = uni_hal_stm32_clock_timeout_begin(UNI_HAL_PWR_VOS_TIMEOUT_MS);
    while (!_uni_hal_stm_pwr_is_voltage_ready()) {
        if (uni_hal_stm32_clock_timeout_expired(&timeout)) {
            return false;
        }
    }

    return true;
}


static bool _uni_hal_stm_pwr_voltagescale(void) {
    // enable LDO
    LL_PWR_ConfigSupply(LL_PWR_LDO_SUPPLY);
    if (!_uni_hal_stm_pwr_wait_vos_ready()) {
        return false;
    }

    // set VOS1
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    if (!_uni_hal_stm_pwr_wait_vos_ready()) {
        return false;
    }

    // enable LDO overdrive
    // event on VOS1 it improves RAM stability on some MCU samples
    SYSCFG->PWRCR |= SYSCFG_PWRCR_ODEN;
    if (!_uni_hal_stm_pwr_wait_vos_ready()) {
        return false;
    }

    // set VOS0
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE0);
    if (!_uni_hal_stm_pwr_wait_vos_ready()) {
        return false;
    }

    return true;
}


//
// Functions
//


bool uni_hal_pwr_init(void) {
    bool result = false;
    result = uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_SYSCFG, true);
    if (!_uni_hal_stm_pwr_voltagescale()) {
        g_uni_hal_pwr_ctx.inited = false;
        g_uni_hal_pwr_ctx.last_state = PWR->CSR1;
        return false;
    }

    g_uni_hal_pwr_ctx.voltage_scale = LL_PWR_GetRegulVoltageScaling();
    g_uni_hal_pwr_ctx.last_state = PWR->CSR1;
    g_uni_hal_pwr_ctx.inited = result;
    return result;
}


bool uni_hal_pwr_set_battery_charging(bool value) {
    // someone else (RTC, LSE set-up) may rely on the backup domain staying writable
    bool const backup_access = LL_PWR_IsEnabledBkUpAccess() != 0U;
    uni_hal_pwr_stm_set_backup_access(true);
    if (value) {
        LL_PWR_SetBattChargResistor(LL_PWR_BATT_CHARGRESISTOR_1_5K);
        LL_PWR_EnableBatteryCharging();
    } else {
        LL_PWR_DisableBatteryCharging();
    }
    uni_hal_pwr_stm_set_backup_access(backup_access);
    g_uni_hal_pwr_ctx.battery_charging = value;

    return true;
}

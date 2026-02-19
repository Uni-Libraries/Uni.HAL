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
#include "dwt/uni_hal_dwt.h"
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

static bool _uni_hal_stm_pwr_is_voltage_ready(void) {
    return (LL_PWR_IsActiveFlag_ACTVOS() != 0U) && (LL_PWR_IsActiveFlag_VOS() != 0U);
}


static bool _uni_hal_stm_pwr_wait_vos_ready(uint32_t timeout_us) {
    if (SystemCoreClock == 0U) {
        return false;
    }

    if (_uni_hal_stm_pwr_is_voltage_ready()) {
        return true;
    }

    if (!uni_hal_dwt_is_inited()) {
        if (!uni_hal_dwt_init()) {
            return false;
        }
    }

    if (timeout_us == 0U) {
        return false;
    }

    uint32_t const start_tick = uni_hal_dwt_get_tick();
    uint64_t const timeout_ticks_64 = ((uint64_t)SystemCoreClock * (uint64_t)timeout_us) / 1'000'000ULL;
    uint32_t timeout_ticks = timeout_ticks_64 > (uint64_t)UINT32_MAX ? UINT32_MAX : (uint32_t)timeout_ticks_64;
    if (timeout_ticks == 0U) {
        timeout_ticks = 1U;
    }

    while (!_uni_hal_stm_pwr_is_voltage_ready()) {
        if (uni_hal_dwt_compare(start_tick, uni_hal_dwt_get_tick()) >= timeout_ticks) {
            return false;
        }
    }

    return true;
}


static bool _uni_hal_stm_pwr_voltagescale(void) {
    uint32_t const pwr_timeout_us = 200'000U;

    // enable LDO
    LL_PWR_ConfigSupply(LL_PWR_LDO_SUPPLY);
    if (!_uni_hal_stm_pwr_wait_vos_ready(pwr_timeout_us)) {
        return false;
    }

    // set VOS1
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    if (!_uni_hal_stm_pwr_wait_vos_ready(pwr_timeout_us)) {
        return false;
    }

    // enable LDO overdrive
    // event on VOS1 it improves RAM stability on some MCU samples
    SYSCFG->PWRCR |= SYSCFG_PWRCR_ODEN;
    if (!_uni_hal_stm_pwr_wait_vos_ready(pwr_timeout_us)) {
        return false;
    }

    // set VOS0
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE0);
    if (!_uni_hal_stm_pwr_wait_vos_ready(pwr_timeout_us)) {
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

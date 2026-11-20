//
// Includes
//

// stdlib
#include <stdint.h>

// ST
#include <stm32l4xx_ll_bus.h>
#include <stm32l4xx_ll_pwr.h>
#include <stm32l4xx_ll_rcc.h>
#include <stm32l4xx_ll_system.h>

// Uni.Common
#include <uni_common.h>

// UNI_HAL
#include "core/uni_hal_core.h"
#include "gpio/uni_hal_gpio.h"
#include "pwr/uni_hal_pwr_stm32.h"
#include "rcc/uni_hal_rcc.h"
#include "rcc/uni_hal_rcc_rtc_stm32_internal.h"
#include "rcc/uni_hal_rcc_stm32l4.h"
#include "systick/uni_hal_systick.h"
#include "systick/uni_hal_systick_stm32.h"


//
// Globals
//

static uni_hal_rcc_stm32l4_config_t rcc_configs[2];
static const uni_hal_rcc_stm32l4_config_t *volatile g_uni_hal_rcc_config;
static volatile uni_hal_rcc_stm32l4_status_t g_uni_hal_rcc_status;
static volatile bool rcc_init_in_progress;
static volatile bool css_during_init;

/**
 * Reset flags (upper byte of RCC_CSR) as found at start-up, before they were cleared
 */
static uint8_t g_uni_hal_rcc_csr_reg = 0U;

UNI_COMMON_COMPILER_WEAK uint32_t SystemCoreClock = 4'000'000U;

UNI_COMMON_COMPILER_WEAK const uint8_t AHBPrescTable[16] = {
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U
};
UNI_COMMON_COMPILER_WEAK const uint8_t APBPrescTable[8] = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};
UNI_COMMON_COMPILER_WEAK const uint32_t MSIRangeTable[12] = {
    100000U, 200000U, 400000U, 800000U, 1000000U, 2000000U,
    4000000U, 8000000U, 16000000U, 24000000U, 32000000U, 48000000U
};


//
// Private
//

typedef uint32_t (*rcc_read_flag_t)(void);

// Flash latency changes take effect within a few bus accesses. PLL lock time is at most
// 40 us (DS11585, table 62). Allow 4096 register polls in NMI without relying on interrupts.
static bool _uni_hal_rcc_poll(rcc_read_flag_t read_flag, uint32_t expected, uint32_t polls) {
    do {
        if (read_flag() == expected) {
            return true;
        }
    } while (--polls);
    return false;
}

static bool _uni_hal_rcc_wait(rcc_read_flag_t read_flag, uint32_t expected, uint32_t timeout_ms, bool recovery) {
    if (recovery) {
        return _uni_hal_rcc_poll(read_flag, expected, timeout_ms ? 4096U : 1U);
    }

    // Also bound startup when the SysTick IRQ is masked. At up to 80 MHz a tick should
    // advance well before 80 000 polls. A working tick retains the configured timeout.
    uint32_t start = uni_hal_systick_get_ms();
    uint32_t last_tick = start;
    uint32_t polls = 0;
    while (read_flag() != expected) {
        uint32_t now = uni_hal_systick_get_ms();
        if (now != last_tick) {
            last_tick = now;
            polls = 0;
        }
        if (now - start >= timeout_ms || ++polls >= 80'000U) {
            return false;
        }
    }
    return true;
}

static uint32_t _uni_hal_rcc_get_pclk1_freq(void) {
    // get HCLK freq
    LL_RCC_ClocksTypeDef clocks;
    LL_RCC_GetSystemClocksFreq(&clocks);
    return clocks.PCLK1_Frequency;
}

static uint32_t _uni_hal_rcc_get_pclk2_freq(void) {
    // get HCLK freq
    LL_RCC_ClocksTypeDef clocks;
    LL_RCC_GetSystemClocksFreq(&clocks);
    return clocks.PCLK2_Frequency;
}

/**
 * Configure FLASH latency
 * TODO: move out
 */
static bool _uni_hal_stm_rcc_flash(uint32_t latency) {
    LL_FLASH_SetLatency(latency);
    return _uni_hal_rcc_poll(LL_FLASH_GetLatency, latency, 16U);
}

/**
 * Lower the flash latency to what the final clock needs.
 * _uni_hal_stm_rcc_flash() sets the 4 wait states of 80 MHz before the clock is switched; once
 * HCLK is known the surplus only slows every flash access down.
 * @param hclk_hz HCLK frequency
 */
static bool _uni_hal_stm_rcc_flash_adjust(uint32_t hclk_hz) {
    // the limits below are those of voltage range 1 (RM0351, table 12)
    if (LL_PWR_GetRegulVoltageScaling() != LL_PWR_REGU_VOLTAGE_SCALE1) {
        return true;
    }

    uint32_t latency = LL_FLASH_LATENCY_4;
    if (hclk_hz <= 16'000'000U) {
        latency = LL_FLASH_LATENCY_0;
    }
    else if (hclk_hz <= 32'000'000U) {
        latency = LL_FLASH_LATENCY_1;
    }
    else if (hclk_hz <= 48'000'000U) {
        latency = LL_FLASH_LATENCY_2;
    }
    else if (hclk_hz <= 64'000'000U) {
        latency = LL_FLASH_LATENCY_3;
    }

    return _uni_hal_stm_rcc_flash(latency);
}


/**
 * Configure high speed external clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_hse() {

    if (g_uni_hal_rcc_config->hse_bypass)
    {
        LL_RCC_HSE_EnableBypass();
    }

    // Enable HSE
    LL_RCC_HSE_Enable();

    // Wait till HSE is ready
    g_uni_hal_rcc_status.hse_inited = _uni_hal_rcc_wait(LL_RCC_HSE_IsReady, 1U, g_uni_hal_rcc_config->timeout.hse, false);
    return g_uni_hal_rcc_status.hse_inited;
}

/**
 * Configure high speed internal clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_hsi(bool recovery) {
    bool result = false;

    LL_RCC_HSI_Enable();

    /* Wait till HSI is ready */
    result = g_uni_hal_rcc_status.hsi_inited = _uni_hal_rcc_wait(LL_RCC_HSI_IsReady, 1U, g_uni_hal_rcc_config->timeout.hsi, recovery);

    if (result) {
        LL_RCC_HSI_SetCalibTrimming(64);
    }

    return result;
}

/**
 * Initialize SysTick
 * @return true on success
 */
static bool _uni_hal_rcc_systick() {
    return uni_hal_systick_init();
}

/**
 * Get the LSE drive level of the configuration
 * @return LL_RCC_LSEDRIVE_xx value
 */
static uint32_t _uni_hal_stm_rcc_lse_drive(void) {
    uint32_t result = LL_RCC_LSEDRIVE_LOW;
    switch (g_uni_hal_rcc_config->lse_drive) {
    case UNI_HAL_RCC_STM32L4_LSE_DRIVE_MEDIUM_LOW:
        result = LL_RCC_LSEDRIVE_MEDIUMLOW;
        break;
    case UNI_HAL_RCC_STM32L4_LSE_DRIVE_MEDIUM_HIGH:
        result = LL_RCC_LSEDRIVE_MEDIUMHIGH;
        break;
    case UNI_HAL_RCC_STM32L4_LSE_DRIVE_HIGH:
        result = LL_RCC_LSEDRIVE_HIGH;
        break;
    case UNI_HAL_RCC_STM32L4_LSE_DRIVE_LOW:
    default:
        break;
    }
    return result;
}


/**
 * Configure low speed external clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_lse() {
    bool result = false;

    // Check LSE_IsReady flag
    result = g_uni_hal_rcc_status.lse_inited = LL_RCC_LSE_IsReady();

    if (!result) {
        uni_hal_pwr_stm_set_backup_access(true);
        LL_RCC_LSE_SetDriveCapability(_uni_hal_stm_rcc_lse_drive());
        LL_RCC_LSE_Enable();

        /* Wait till LSE is ready */
        result = g_uni_hal_rcc_status.lse_inited = _uni_hal_rcc_wait(LL_RCC_LSE_IsReady, 1U, g_uni_hal_rcc_config->timeout.lse, false);
        uni_hal_pwr_stm_set_backup_access(false);

        // Try one time to reset Backup Domain and init LSE again
        if (!result && g_uni_hal_rcc_config->lse_backup_reset && !g_uni_hal_rcc_status.lse_backup_reseted) {
            uni_hal_rcc_reset(UNI_HAL_RCC_RESET_BACKUP);
            g_uni_hal_rcc_status.lse_backup_reseted = true;
            result = _uni_hal_stm_rcc_lse();
        }
    }


    return result;
}

/**
 * Configure low speed internal clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_lsi() {
    bool result = false;

    LL_RCC_LSI_Enable();

    // Wait till LSE is ready
    result = g_uni_hal_rcc_status.lsi_inited = _uni_hal_rcc_wait(LL_RCC_LSI_IsReady, 1U, g_uni_hal_rcc_config->timeout.lsi, false);


    return result;
}

// Convert the board's HSE-based PLL configuration to legal factors for the selected source.
static bool _uni_hal_rcc_pll_factors(uni_hal_rcc_stm32l4_config_pll_t *pll, uint32_t source) {
    if (pll->m < 1 || pll->m > 8 || pll->n < 8 || pll->n > 86 ||
        pll->p == 1 || pll->p > 31 || pll->q > 8 || pll->q % 2 != 0 ||
        pll->r < 2 || pll->r > 8 || pll->r % 2 != 0) {
        return false;
    }

    // Validate against DS11585 table 62 without rounding a fractional clock down.
    uint64_t vco = (uint64_t)HSE_VALUE * pll->n;  // VCO frequency times M
    bool range1 = LL_PWR_GetRegulVoltageScaling() == LL_PWR_REGU_VOLTAGE_SCALE1;
    uint32_t max_vco = range1 ? 344'000'000U : 128'000'000U;
    uint32_t max_output = range1 ? 80'000'000U : 26'000'000U;
    if (HSE_VALUE < 4'000'000U * pll->m || HSE_VALUE > 16'000'000U * pll->m ||
        vco < 64'000'000U * pll->m || vco > (uint64_t)max_vco * pll->m ||
        (pll->p && vco > (uint64_t)max_output * pll->m * pll->p) ||
        (pll->q && vco > (uint64_t)max_output * pll->m * pll->q) ||
        vco > (uint64_t)max_output * pll->m * pll->r) {
        return false;
    }
    if (source == LL_RCC_PLLSOURCE_HSE) {
        return true;
    }

    // Keep the VCO (and therefore every output) exact. Prefer the original N.
    uint64_t scaled_m = (uint64_t)HSI_VALUE * pll->m;
    if (scaled_m % HSE_VALUE == 0 && scaled_m / HSE_VALUE <= 8) {
        pll->m = (uint32_t)(scaled_m / HSE_VALUE);
        return true;
    }
    for (uint32_t m = 1; m <= 8; ++m) {
        if (HSI_VALUE < 4'000'000U * m || HSI_VALUE > 16'000'000U * m) {
            continue;
        }
        uint64_t scaled_n = vco * m;
        uint32_t n = (uint32_t)(scaled_n / scaled_m);
        if (scaled_n % scaled_m == 0 && n >= 8 && n <= 86) {
            pll->m = m;
            pll->n = n;
            return true;
        }
    }
    return false;
}

// PLLPDIV=0 selects the fixed divider in PLLP. A bootloader may use either encoding.
static uint32_t _uni_hal_rcc_pll_p_divisor(void) {
    uint32_t divisor = LL_RCC_PLL_GetP() >> RCC_PLLCFGR_PLLPDIV_Pos;
    return divisor ? divisor : ((RCC->PLLCFGR & RCC_PLLCFGR_PLLP) ? 17U : 7U);
}

static bool _uni_hal_stm_rcc_pll(bool recovery) {
    g_uni_hal_rcc_status.pll_inited = false;
    uint32_t source = g_uni_hal_rcc_status.hse_inited ? LL_RCC_PLLSOURCE_HSE : LL_RCC_PLLSOURCE_HSI;
    uni_hal_rcc_stm32l4_config_pll_t pll = g_uni_hal_rcc_config->pll[0];
    if ((!g_uni_hal_rcc_status.hse_inited && !g_uni_hal_rcc_status.hsi_inited) ||
        !_uni_hal_rcc_pll_factors(&pll, source)) {
        return false;
    }
    uint32_t divider = (pll.m - 1U) << RCC_PLLCFGR_PLLM_Pos;
    uint32_t q_bits = pll.q ? (pll.q / 2U - 1U) << RCC_PLLCFGR_PLLQ_Pos : 0U;
    uint32_t output = (pll.r / 2U - 1U) << RCC_PLLCFGR_PLLR_Pos;
    uint32_t timeout = g_uni_hal_rcc_config->timeout.pll;
    if (LL_RCC_GetSysClkSource() == LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {
        // A running system PLL cannot be disabled or rewritten.
        g_uni_hal_rcc_status.pll_inited = LL_RCC_PLL_IsReady() && LL_RCC_PLL_GetMainSource() == source &&
            LL_RCC_PLL_GetDivider() == divider && LL_RCC_PLL_GetN() == pll.n && LL_RCC_PLL_GetR() == output &&
            (!pll.p || (LL_RCC_PLL_IsEnabledDomain_SAI() && _uni_hal_rcc_pll_p_divisor() == pll.p)) &&
            (!pll.q || (LL_RCC_PLL_IsEnabledDomain_48M() && LL_RCC_PLL_GetQ() == q_bits));
        return g_uni_hal_rcc_status.pll_inited;
    }
    LL_RCC_PLL_Disable();
    if (!_uni_hal_rcc_wait(LL_RCC_PLL_IsReady, 0U, timeout, recovery)) {
        return false;
    }
    // Source and M are shared with both SAI PLLs and are locked while either runs.
    if ((RCC->CR & (RCC_CR_PLLSAI1ON | RCC_CR_PLLSAI1RDY | RCC_CR_PLLSAI2ON | RCC_CR_PLLSAI2RDY)) &&
        (LL_RCC_PLL_GetMainSource() != source || LL_RCC_PLL_GetDivider() != divider)) {
        return false;
    }
    LL_RCC_PLL_ConfigDomain_SYS(source, divider, pll.n, output);
    MODIFY_REG(RCC->PLLCFGR, RCC_PLLCFGR_PLLPDIV | RCC_PLLCFGR_PLLPEN | RCC_PLLCFGR_PLLQ | RCC_PLLCFGR_PLLQEN,
        (pll.p << RCC_PLLCFGR_PLLPDIV_Pos) | (pll.p ? RCC_PLLCFGR_PLLPEN : 0U) |
        q_bits | (pll.q ? RCC_PLLCFGR_PLLQEN : 0U));
    LL_RCC_PLL_EnableDomain_SYS();
    LL_RCC_PLL_Enable();
    g_uni_hal_rcc_status.pll_inited = _uni_hal_rcc_wait(LL_RCC_PLL_IsReady, 1U, timeout, recovery);
    if (!g_uni_hal_rcc_status.pll_inited) {
        LL_RCC_PLL_Disable();
    }
    return g_uni_hal_rcc_status.pll_inited;
}

// Rescale the existing time base after a clock change, including the automatic CSS switch.
// Leave NVIC priority, interrupt enable and clock selection under their current owner's control.
static void _uni_hal_rcc_update_timebase(void) {
    uint32_t const old_hclk = SystemCoreClock;
    SystemCoreClockUpdate();
    uni_hal_systick_reclock(old_hclk, SystemCoreClock);
}

static bool _uni_hal_stm_rcc_sysclk(bool recovery) {
    uint32_t source;
    uint32_t expected;
    g_uni_hal_rcc_status.sys_inited = false;
    if (g_uni_hal_rcc_status.pll_inited) {
        source = LL_RCC_SYS_CLKSOURCE_PLL;
        expected = LL_RCC_SYS_CLKSOURCE_STATUS_PLL;
    } else if (g_uni_hal_rcc_status.hse_inited && HSE_VALUE <= 48'000'000U &&
               (LL_PWR_GetRegulVoltageScaling() == LL_PWR_REGU_VOLTAGE_SCALE1 || HSE_VALUE <= 26'000'000U)) {
        source = LL_RCC_SYS_CLKSOURCE_HSE;
        expected = LL_RCC_SYS_CLKSOURCE_STATUS_HSE;
    } else if (g_uni_hal_rcc_status.hsi_inited) {
        source = LL_RCC_SYS_CLKSOURCE_HSI;
        expected = LL_RCC_SYS_CLKSOURCE_STATUS_HSI;
    } else {
        _uni_hal_rcc_update_timebase();
        return false;
    }

    LL_RCC_SetSysClkSource(source);
    if (!_uni_hal_rcc_wait(LL_RCC_GetSysClkSource, expected, g_uni_hal_rcc_config->timeout.pll, recovery)) {
        // Cancel an unacknowledged request so the clock cannot switch after we return.
        LL_RCC_SetSysClkSource(LL_RCC_GetSysClkSource() >> RCC_CFGR_SWS_Pos);
        _uni_hal_rcc_update_timebase();
        return false;
    }

    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
    _uni_hal_rcc_update_timebase();
    g_uni_hal_rcc_status.sys_inited = _uni_hal_stm_rcc_flash_adjust(SystemCoreClock);
    return g_uni_hal_rcc_status.sys_inited;
}

static bool _uni_hal_rcc_pllq_is_48mhz(void) {
    if (!LL_RCC_PLL_IsReady() || !LL_RCC_PLL_IsEnabledDomain_48M()) {
        return false;
    }
    uint32_t input;
    switch (LL_RCC_PLL_GetMainSource()) {
    case LL_RCC_PLLSOURCE_HSE:
        input = HSE_VALUE;
        break;
    case LL_RCC_PLLSOURCE_HSI:
        input = HSI_VALUE;
        break;
    case LL_RCC_PLLSOURCE_MSI: {
        uint32_t range_select = LL_RCC_MSI_IsEnabledRangeSelect();
        uint32_t range = range_select ? LL_RCC_MSI_GetRange() : LL_RCC_MSI_GetRangeAfterStandby();
        input = __LL_RCC_CALC_MSI_FREQ(range_select, range);
        break;
    }
    default:
        return false;
    }
    uint32_t m = 1U + (LL_RCC_PLL_GetDivider() >> RCC_PLLCFGR_PLLM_Pos);
    uint32_t q = 2U * (1U + (LL_RCC_PLL_GetQ() >> RCC_PLLCFGR_PLLQ_Pos));
    return (uint64_t)input * LL_RCC_PLL_GetN() == (uint64_t)HSI48_VALUE * m * q;
}

static void _uni_hal_rcc_select_clk48(void) {
    uint32_t current = LL_RCC_GetRNGClockSource(LL_RCC_RNG_CLKSOURCE);
    // USB and SDMMC share this mux. Preserve a running oscillator or PLLSAI1 set up by the board.
    if (current != LL_RCC_RNG_CLKSOURCE_PLL && LL_RCC_GetRNGClockFreq(LL_RCC_RNG_CLKSOURCE) != 0U) {
        return;
    }
    uint32_t source = _uni_hal_rcc_pllq_is_48mhz() ? LL_RCC_RNG_CLKSOURCE_PLL : LL_RCC_RNG_CLKSOURCE_HSI48;
    if (source != current) {
        if (source == LL_RCC_RNG_CLKSOURCE_HSI48) {
            // A previously selected clock was lost. Resume its consumers without waiting in NMI.
            LL_RCC_HSI48_Enable();
        }
        LL_RCC_SetRNGClockSource(source);
    }
}

static bool _uni_hal_rcc_recover(void) {
    g_uni_hal_rcc_status.hse_inited = false;
    g_uni_hal_rcc_status.pll_inited = false;
    g_uni_hal_rcc_status.sys_inited = false;
    _uni_hal_stm_rcc_hsi(true);
    bool recovered = false;
    if (_uni_hal_stm_rcc_flash(LL_FLASH_LATENCY_4)) {
        _uni_hal_stm_rcc_pll(true);
        recovered = _uni_hal_stm_rcc_sysclk(true);
    } else {
        _uni_hal_rcc_update_timebase();
    }
    _uni_hal_rcc_select_clk48();
    return recovered;
}

/**
 * Configure peripherals
 * @param ctx RCC context
 */
static void _uni_hal_stm_rcc_peripherials() {
    LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_SYSCLK);

    LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_PCLK1);
    LL_RCC_SetI2CClockSource(LL_RCC_I2C2_CLKSOURCE_PCLK1);
    LL_RCC_SetI2CClockSource(LL_RCC_I2C3_CLKSOURCE_PCLK1);
    LL_RCC_SetI2CClockSource(LL_RCC_I2C4_CLKSOURCE_PCLK1);

    _uni_hal_rcc_select_clk48();

    LL_RCC_SetUARTClockSource(LL_RCC_UART4_CLKSOURCE_PCLK1);

    LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);
    LL_RCC_SetUSARTClockSource(LL_RCC_USART2_CLKSOURCE_PCLK1);
}

static bool _uni_hal_rcc_update_status_reg() {
    bool result = false;
    g_uni_hal_rcc_csr_reg = (uint8_t) ((RCC->CSR) >> 24);
    LL_RCC_ClearResetFlags();
    result = true;

    return result;
}

static uint32_t _uni_hal_rcc_apb1_prescaler_get(){
    uint32_t result = 1;

    switch(LL_RCC_GetAPB1Prescaler()){
        case LL_RCC_APB1_DIV_1:
            result = 1;
            break;
        case LL_RCC_APB1_DIV_2:
            result = 2;
            break;
        case LL_RCC_APB1_DIV_4:
            result = 4;
            break;
        case LL_RCC_APB1_DIV_8:
            result = 8;
            break;
        case LL_RCC_APB1_DIV_16:
            result = 16;
            break;
    }

    return result;
}

static uint32_t _uni_hal_rcc_apb2_prescaler_get(){
    uint32_t result = 1;

    switch(LL_RCC_GetAPB2Prescaler()){
        case LL_RCC_APB2_DIV_1:
            result = 1;
            break;
        case LL_RCC_APB2_DIV_2:
            result = 2;
            break;
        case LL_RCC_APB2_DIV_4:
            result = 4;
            break;
        case LL_RCC_APB2_DIV_8:
            result = 8;
            break;
        case LL_RCC_APB2_DIV_16:
            result = 16;
            break;
    }

    return result;
}


//
// Public
//

bool uni_hal_rcc_init() {
    if (!g_uni_hal_rcc_config) {
        return false;
    }

    bool was_inited = g_uni_hal_rcc_status.inited;
    css_during_init = false;
    rcc_init_in_progress = true;
    g_uni_hal_rcc_status = (uni_hal_rcc_stm32l4_status_t){
        .lse_backup_reseted = g_uni_hal_rcc_status.lse_backup_reseted
    };

    // enable configuration clocking
    uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_SYSCFG, true);
    uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_PWR, true);

    // set flash latency
    if (!_uni_hal_stm_rcc_flash(LL_FLASH_LATENCY_4) || (!was_inited && !_uni_hal_rcc_systick())) {
        rcc_init_in_progress = false;
        if (css_during_init) {
            _uni_hal_rcc_update_timebase();
        }
        return false;
    }

    // basic settings
    _uni_hal_rcc_update_status_reg();
    // TODO: move to another file

    // HSE
    if (g_uni_hal_rcc_config->hse_enable)
    {
        _uni_hal_stm_rcc_hse();
    }

    // HSI
    _uni_hal_stm_rcc_hsi(false);

    // LSE
    if (g_uni_hal_rcc_config->lse_enable) {
        _uni_hal_stm_rcc_lse();
    }

    // LSI, with or without LSE: the independent watchdog runs from it
    _uni_hal_stm_rcc_lsi();

    // PLL
    _uni_hal_stm_rcc_pll(false);

    // Arm CSS before switching away from the current system clock. If the NMI
    // interrupts this sequence, it records the fault and this invocation owns recovery.
    if (g_uni_hal_rcc_status.hse_inited && g_uni_hal_rcc_config->hse_css) {
        LL_RCC_HSE_EnableCSS();
    }
    if (_uni_hal_stm_rcc_sysclk(false)) {
        _uni_hal_stm_rcc_peripherials();
        g_uni_hal_rcc_status.inited = true;
    }
    rcc_init_in_progress = false;
    if (css_during_init) {
        g_uni_hal_rcc_status.inited = _uni_hal_rcc_recover();
        if (g_uni_hal_rcc_status.inited) {
            _uni_hal_stm_rcc_peripherials();
        }
    }
    return g_uni_hal_rcc_status.inited;
}


bool uni_hal_rcc_is_inited() {
    return g_uni_hal_rcc_status.inited;
}

uint8_t uni_hal_rcc_get_status_reg() {
    uint8_t result = 0;
    if (uni_hal_rcc_is_inited()) {
        result = g_uni_hal_rcc_csr_reg;
    }
    return result;
}


bool uni_hal_rcc_get_status_reg_flag(uint8_t flag) {
    bool result = false;
    if (uni_hal_rcc_is_inited()) {
        result = (g_uni_hal_rcc_csr_reg & flag) != 0U;
    }
    return result;
}


bool uni_hal_rcc_reset(uni_hal_rcc_reset_target_e target) {
    bool result = false;

    switch (target) {
        case UNI_HAL_RCC_RESET_BACKUP:
            uni_hal_pwr_stm_set_backup_access(true);
        LL_RCC_ForceBackupDomainReset();
        LL_RCC_ReleaseBackupDomainReset();
        uni_hal_pwr_stm_set_backup_access(false);
        g_uni_hal_rcc_status.lse_backup_reseted = true;
        result = true;
        break;
        default:
            break;
    }

    return result;
}


//
// Clock
//

bool uni_hal_rcc_clk_get(uni_hal_core_periph_e clock) {
    bool result = false;

    switch (clock) {
        case UNI_HAL_CORE_PERIPH_ADC_1:
        case UNI_HAL_CORE_PERIPH_ADC_2:
        case UNI_HAL_CORE_PERIPH_ADC_3:
            result = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_ADC);
            break;
        case UNI_HAL_CORE_PERIPH_CAN_1:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_CAN1);
            break;
        case UNI_HAL_CORE_PERIPH_CAN_2:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_CAN2);
            break;
        case UNI_HAL_CORE_PERIPH_CRC_1:
            result = LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_CRC);
            break;
        case UNI_HAL_CORE_PERIPH_DAC_1:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_DAC1);
            break;
        case UNI_HAL_CORE_PERIPH_DMA_1:
            result = LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_DMA1);
            break;
        case UNI_HAL_CORE_PERIPH_DMA_2:
            result = LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_DMA2);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_A:
            result = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOA);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_B:
            result = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOB);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_C:
            result = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOC);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_D:
            result = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOD);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_E:
            result = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOE);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_F:
            result = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOF);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_G:
            result = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOG);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_H:
            result = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOH);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_I:
            result = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOI);
            break;
        case UNI_HAL_CORE_PERIPH_I2C_1:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_I2C1);
            break;
        case UNI_HAL_CORE_PERIPH_I2C_2:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_I2C2);
            break;
        case UNI_HAL_CORE_PERIPH_I2C_3:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_I2C3);
            break;
        case UNI_HAL_CORE_PERIPH_I2C_4:
            result = LL_APB1_GRP2_IsEnabledClock(LL_APB1_GRP2_PERIPH_I2C4);
            break;
        case UNI_HAL_CORE_PERIPH_LPUART_1:
            result = LL_APB1_GRP2_IsEnabledClock(LL_APB1_GRP2_PERIPH_LPUART1);
            break;
        case UNI_HAL_CORE_PERIPH_PWR:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_PWR);
            break;
        case UNI_HAL_CORE_PERIPH_RNG:
            result = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_RNG);
            break;
        case UNI_HAL_CORE_PERIPH_RTC:
            result = LL_RCC_IsEnabledRTC();
            break;
        case UNI_HAL_CORE_PERIPH_SPI_1:
            result = LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_SPI1);
            break;
        case UNI_HAL_CORE_PERIPH_SPI_2:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_SPI2);
            break;
        case UNI_HAL_CORE_PERIPH_SPI_3:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_SPI3);
            break;
        case UNI_HAL_CORE_PERIPH_SYSCFG:
            result = LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_SYSCFG);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_1:
            result = LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_TIM1);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_2:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_TIM2);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_3:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_TIM3);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_4:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_TIM4);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_5:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_TIM5);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_6:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_TIM6);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_7:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_TIM7);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_8:
            result = LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_TIM8);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_15:
            result = LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_TIM15);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_16:
            result = LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_TIM16);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_17:
            result = LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_TIM17);
            break;
        case UNI_HAL_CORE_PERIPH_UART_1:
            result = LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_USART1);
            break;
        case UNI_HAL_CORE_PERIPH_UART_2:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_USART2);
            break;
        case UNI_HAL_CORE_PERIPH_UART_3:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_USART3);
            break;
        case UNI_HAL_CORE_PERIPH_UART_4:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_UART4);
            break;
        case UNI_HAL_CORE_PERIPH_UART_5:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_UART5);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_9:
        case UNI_HAL_CORE_PERIPH_TIM_10:
        case UNI_HAL_CORE_PERIPH_TIM_11:
        case UNI_HAL_CORE_PERIPH_TIM_12:
        case UNI_HAL_CORE_PERIPH_TIM_13:
        case UNI_HAL_CORE_PERIPH_TIM_14:
        default:
            break;
    }

    return result;
}


bool uni_hal_rcc_clk_set(uni_hal_core_periph_e target, bool state) {
    bool result = false;
    if (target == UNI_HAL_CORE_PERIPH_RTC) {
        result = set_rtc_clock_enabled(state);
    } else {
        switch (target) {
            case UNI_HAL_CORE_PERIPH_ADC_1:
            case UNI_HAL_CORE_PERIPH_ADC_2:
            case UNI_HAL_CORE_PERIPH_ADC_3:
                state
                    ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_ADC)
                    : LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_ADC);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_CAN_1:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_CAN1)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_CAN1);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_CAN_2:
                if (state) {
                    //CAN1 must be also enabled to bring CAN2 into the working state
                    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_CAN1);
                    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_CAN2);
                } else {
                    LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_CAN2);
                }
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_CRC_1:
                state
                    ? LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CRC)
                    : LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_CRC);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_DAC_1:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_DAC1);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_DMA_1:
                state
                    ? LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1)
                    : LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_DMA1);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_DMA_2:
                state
                    ? LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2)
                    : LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_DMA2);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_GPIO_A:
                state
                    ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA)
                    : LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_GPIO_B:
                state
                    ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB)
                    : LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_GPIO_C:
                state
                    ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC)
                    : LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_GPIO_D:
                state
                    ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOD)
                    : LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOD);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_GPIO_E:
                state
                    ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOE)
                    : LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOE);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_GPIO_F:
                state
                    ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOF)
                    : LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOF);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_GPIO_G:
                state
                    ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOG)
                    : LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOG);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_GPIO_H:
                state
                    ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOH)
                    : LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOH);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_GPIO_I:
                state
                    ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOI)
                    : LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOI);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_I2C_1:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_I2C1);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_I2C_2:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_I2C2);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_I2C_3:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C3)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_I2C3);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_I2C_4:
                state
                    ? LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_I2C4)
                    : LL_APB1_GRP2_DisableClock(LL_APB1_GRP2_PERIPH_I2C4);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_LPUART_1:
            state
                ? LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_LPUART1)
                : LL_APB1_GRP2_DisableClock(LL_APB1_GRP2_PERIPH_LPUART1);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_PWR:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_PWR);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_RNG:
                state
                    ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_RNG)
                    : LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_RNG);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_SPI_1:
                state
                    ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1)
                    : LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_SPI1);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_SPI_2:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_SPI2);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_SPI_3:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI3)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_SPI3);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_SYSCFG:
                state
                    ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG)
                    : LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_1:
                state
                    ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1)
                    : LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_TIM1);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_2:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_TIM2);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_3:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_TIM3);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_4:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_TIM4);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_5:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM5)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_TIM5);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_6:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM6)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_TIM6);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_7:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM7)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_TIM7);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_8:
                state
                    ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM8)
                    : LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_TIM8);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_15:
                state
                    ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM15)
                    : LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_TIM15);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_16:
                state
                    ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM16)
                    : LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_TIM16);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_17:
                state
                    ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM17)
                    : LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_TIM17);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_UART_1:
                state
                    ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1)
                    : LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_USART1);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_UART_2:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_USART2);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_UART_3:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_USART3);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_UART_4:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART4)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_UART4);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_UART_5:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART5)
                    : LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_UART5);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_TIM_9:
            case UNI_HAL_CORE_PERIPH_TIM_10:
            case UNI_HAL_CORE_PERIPH_TIM_11:
            case UNI_HAL_CORE_PERIPH_TIM_12:
            case UNI_HAL_CORE_PERIPH_TIM_13:
            case UNI_HAL_CORE_PERIPH_TIM_14:
            default:
                break;
        }
    }

    return  result;
}

uint32_t uni_hal_rcc_clk_get_freq(uni_hal_core_periph_e target) {
    uint32_t result = 0U;
    if (uni_hal_rcc_is_inited()) {
        switch (target) {
            case UNI_HAL_CORE_PERIPH_SYSCLK: {
                LL_RCC_ClocksTypeDef clocks;
                LL_RCC_GetSystemClocksFreq(&clocks);
                result = clocks.SYSCLK_Frequency;
                break;
            }
            case UNI_HAL_CORE_PERIPH_CAN_1:
            case UNI_HAL_CORE_PERIPH_CAN_2:
                result = _uni_hal_rcc_get_pclk1_freq();
                break;
            case UNI_HAL_CORE_PERIPH_I2C_1:
                result = LL_RCC_GetI2CClockFreq(LL_RCC_I2C1_CLKSOURCE);
                break;
            case UNI_HAL_CORE_PERIPH_I2C_2:
                result = LL_RCC_GetI2CClockFreq(LL_RCC_I2C2_CLKSOURCE);
                break;
            case UNI_HAL_CORE_PERIPH_I2C_3:
                result = LL_RCC_GetI2CClockFreq(LL_RCC_I2C3_CLKSOURCE);
                break;
            case UNI_HAL_CORE_PERIPH_I2C_4:
                result = LL_RCC_GetI2CClockFreq(LL_RCC_I2C4_CLKSOURCE);
                break;
            case UNI_HAL_CORE_PERIPH_LPUART_1:
                result = LL_RCC_GetLPUARTClockFreq(LL_RCC_LPUART1_CLKSOURCE);
                break;
            case UNI_HAL_CORE_PERIPH_UART_1:
                result = LL_RCC_GetUSARTClockFreq(LL_RCC_USART1_CLKSOURCE);
                break;
            case UNI_HAL_CORE_PERIPH_UART_2:
                result = LL_RCC_GetUSARTClockFreq(LL_RCC_USART2_CLKSOURCE);
                break;
            case UNI_HAL_CORE_PERIPH_UART_3:
                result = LL_RCC_GetUSARTClockFreq(LL_RCC_USART3_CLKSOURCE);
                break;
            case UNI_HAL_CORE_PERIPH_UART_4:
                result = LL_RCC_GetUARTClockFreq(LL_RCC_UART4_CLKSOURCE);
                break;
            case UNI_HAL_CORE_PERIPH_UART_5:
                result = LL_RCC_GetUARTClockFreq(LL_RCC_UART5_CLKSOURCE);
                break;
            case UNI_HAL_CORE_PERIPH_TIM_1:
            case UNI_HAL_CORE_PERIPH_TIM_8:
            case UNI_HAL_CORE_PERIPH_TIM_15:
            case UNI_HAL_CORE_PERIPH_TIM_16:
            case UNI_HAL_CORE_PERIPH_TIM_17:
                result = _uni_hal_rcc_get_pclk2_freq() * ((_uni_hal_rcc_apb2_prescaler_get() > 1) ? 2 : 1);
                break;
            case UNI_HAL_CORE_PERIPH_TIM_2:
            case UNI_HAL_CORE_PERIPH_TIM_3:
            case UNI_HAL_CORE_PERIPH_TIM_4:
            case UNI_HAL_CORE_PERIPH_TIM_5:
            case UNI_HAL_CORE_PERIPH_TIM_6:
            case UNI_HAL_CORE_PERIPH_TIM_7:
                result = _uni_hal_rcc_get_pclk1_freq() * ((_uni_hal_rcc_apb1_prescaler_get() > 1) ? 2 : 1);
                break;
            case UNI_HAL_CORE_PERIPH_TIM_9:
            case UNI_HAL_CORE_PERIPH_TIM_10:
            case UNI_HAL_CORE_PERIPH_TIM_11:
            case UNI_HAL_CORE_PERIPH_TIM_12:
            case UNI_HAL_CORE_PERIPH_TIM_13:
            case UNI_HAL_CORE_PERIPH_TIM_14:
                result = 0;
                break;
            default:
                break;
        }
    }

    return result;
}


//
// Clock Source
//

uni_hal_rcc_clksrc_e uni_hal_rcc_clksrc_get(uni_hal_core_periph_e target) {
    uni_hal_rcc_clksrc_e result = UNI_HAL_RCC_CLKSRC_UNKNOWN;
    if (uni_hal_rcc_is_inited()) {
        switch (target) {
            case UNI_HAL_CORE_PERIPH_ADC_1:
            case UNI_HAL_CORE_PERIPH_ADC_2:
            case UNI_HAL_CORE_PERIPH_ADC_3:
                switch (LL_RCC_GetADCClockSource(LL_RCC_ADC_CLKSOURCE)) {
                    case LL_RCC_ADC_CLKSOURCE_SYSCLK: return UNI_HAL_RCC_CLKSRC_SYSCLK;
                    case LL_RCC_ADC_CLKSOURCE_NONE: return UNI_HAL_RCC_CLKSRC_NONE;
                    default: return UNI_HAL_RCC_CLKSRC_UNKNOWN;
                }
            case UNI_HAL_CORE_PERIPH_RTC:
                switch (LL_RCC_GetRTCClockSource()) {
                    case LL_RCC_RTC_CLKSOURCE_NONE:
                        result = UNI_HAL_RCC_CLKSRC_NONE;
                        break;
                    case LL_RCC_RTC_CLKSOURCE_LSE:
                        result = UNI_HAL_RCC_CLKSRC_LSE;
                        break;
                    case LL_RCC_RTC_CLKSOURCE_LSI:
                        result = UNI_HAL_RCC_CLKSRC_LSI;
                        break;
                    case LL_RCC_RTC_CLKSOURCE_HSE_DIV32:
                        result = UNI_HAL_RCC_CLKSRC_HSE_DIV_32;
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
    return result;
}


bool uni_hal_rcc_clksrc_set(uni_hal_core_periph_e target, uni_hal_rcc_clksrc_e source) {
    bool result = false;

    if (uni_hal_rcc_is_inited()) {
        switch (target) {
            case UNI_HAL_CORE_PERIPH_I2C_1:
            case UNI_HAL_CORE_PERIPH_I2C_2:
            case UNI_HAL_CORE_PERIPH_I2C_3:
            case UNI_HAL_CORE_PERIPH_I2C_4: {
                static const uint32_t pclk1[] = {LL_RCC_I2C1_CLKSOURCE_PCLK1, LL_RCC_I2C2_CLKSOURCE_PCLK1,
                                                LL_RCC_I2C3_CLKSOURCE_PCLK1, LL_RCC_I2C4_CLKSOURCE_PCLK1};
                static const uint32_t sysclk[] = {LL_RCC_I2C1_CLKSOURCE_SYSCLK, LL_RCC_I2C2_CLKSOURCE_SYSCLK,
                                                 LL_RCC_I2C3_CLKSOURCE_SYSCLK, LL_RCC_I2C4_CLKSOURCE_SYSCLK};
                static const uint32_t hsi[] = {LL_RCC_I2C1_CLKSOURCE_HSI, LL_RCC_I2C2_CLKSOURCE_HSI,
                                              LL_RCC_I2C3_CLKSOURCE_HSI, LL_RCC_I2C4_CLKSOURCE_HSI};
                const unsigned index = target - UNI_HAL_CORE_PERIPH_I2C_1;
                uint32_t mux;
                switch (source) {
                    case UNI_HAL_RCC_CLKSRC_PCLK1:  mux = pclk1[index]; break;
                    case UNI_HAL_RCC_CLKSRC_SYSCLK: mux = sysclk[index]; break;
                    case UNI_HAL_RCC_CLKSRC_HSI:    mux = hsi[index]; break;
                    default: return false;
                }
                // I2C1..3 share CCIPR. Keep their read-modify-write operations indivisible.
                uint32_t const mask = uni_hal_core_irq_pause();
                LL_RCC_SetI2CClockSource(mux);
                uni_hal_core_irq_resume(mask);
                result = true;
                break;
            }
            case UNI_HAL_CORE_PERIPH_LPUART_1: {
                    uint32_t src = LL_RCC_LPUART1_CLKSOURCE_SYSCLK;
                    if (source == UNI_HAL_RCC_CLKSRC_PCLK1) {
                        src = LL_RCC_LPUART1_CLKSOURCE_PCLK1;
                    } else if (source == UNI_HAL_RCC_CLKSRC_SYSCLK) {
                        src = LL_RCC_LPUART1_CLKSOURCE_SYSCLK;
                    } else if (source == UNI_HAL_RCC_CLKSRC_HSI) {
                        src = LL_RCC_LPUART1_CLKSOURCE_HSI;
                    } else if (source == UNI_HAL_RCC_CLKSRC_LSE){
                        src = LL_RCC_LPUART1_CLKSOURCE_LSE;
                    }
                    LL_RCC_SetLPUARTClockSource(src);
                    result = true;
                    break;
            }
            case UNI_HAL_CORE_PERIPH_RTC:
                return select_rtc_source(source);
            case UNI_HAL_CORE_PERIPH_ADC_1:
            case UNI_HAL_CORE_PERIPH_ADC_2:
            case UNI_HAL_CORE_PERIPH_ADC_3:
                // one kernel clock for the three ADC; PLLSAI1 and PLLSAI2 are not set up by this driver
                if (source == UNI_HAL_RCC_CLKSRC_SYSCLK) {
                    LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_SYSCLK);
                    result = true;
                } else if (source == UNI_HAL_RCC_CLKSRC_NONE) {
                    LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_NONE);
                    result = true;
                }
                break;
            default:
                break;
        }
    }

    return result;
}

uni_hal_rcc_stm32l4_status_t uni_hal_rcc_stm32l4_status_get(void) {
    return g_uni_hal_rcc_status;
}


bool uni_hal_rcc_stm32l4_config_set(uni_hal_rcc_stm32l4_config_t* config)
{
    if (!config) {
        return false;
    }
    // NMI may preempt the copy. Publish a complete snapshot with one pointer store.
    uni_hal_rcc_stm32l4_config_t *next = g_uni_hal_rcc_config == &rcc_configs[0] ? &rcc_configs[1] : &rcc_configs[0];
    *next = *config;
    __DMB();
    g_uni_hal_rcc_config = next;
    return true;
}


uint32_t uni_hal_rcc_stm32_mco_enable(uint32_t mco_index, uni_hal_rcc_clksrc_e clock_source, uint32_t clock_divider) {
    if (mco_index != 0) {
        return 0U;
    }

    uint32_t source;
    uint32_t prescaler;
    switch (clock_source) {
        case UNI_HAL_RCC_CLKSRC_HSI:
            source = LL_RCC_MCO1SOURCE_HSI;
            break;
        case UNI_HAL_RCC_CLKSRC_LSE:
            source = LL_RCC_MCO1SOURCE_LSE;
            break;
        case UNI_HAL_RCC_CLKSRC_HSE:
            source = LL_RCC_MCO1SOURCE_HSE;
            break;
        case UNI_HAL_RCC_CLKSRC_PLL1R:
            source = LL_RCC_MCO1SOURCE_PLLCLK;
            break;
        case UNI_HAL_RCC_CLKSRC_HSI48:
            source = LL_RCC_MCO1SOURCE_HSI48;
            break;
        case UNI_HAL_RCC_CLKSRC_SYSCLK:
            source = LL_RCC_MCO1SOURCE_SYSCLK;
            break;
        case UNI_HAL_RCC_CLKSRC_MSI:
            source = LL_RCC_MCO1SOURCE_MSI;
            break;
        case UNI_HAL_RCC_CLKSRC_LSI:
            source = LL_RCC_MCO1SOURCE_LSI;
            break;
        default:
            return 0U;
    }

    switch (clock_divider) {
        case 1:
            prescaler = LL_RCC_MCO1_DIV_1;
            break;
        case 2:
            prescaler = LL_RCC_MCO1_DIV_2;
            break;
        case 4:
            prescaler = LL_RCC_MCO1_DIV_4;
            break;
        case 8:
            prescaler = LL_RCC_MCO1_DIV_8;
            break;
        case 16:
            prescaler = LL_RCC_MCO1_DIV_16;
            break;
        default:
            return 0U;
    }

    uni_hal_gpio_pin_context_t pin = {
        .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_A,
        .gpio_pin = UNI_HAL_GPIO_PIN_8,
        .gpio_speed = UNI_HAL_GPIO_SPEED_3,
        .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
        .alternate = UNI_HAL_GPIO_ALTERNATE_0,
    };
    if (!uni_hal_gpio_pin_init(&pin)) {
        return 0U;
    }

    LL_RCC_ConfigMCO(source, prescaler);
    return 1U;
}


//
// STM32L4 - Interrupts
//

UNI_COMMON_COMPILER_WEAK void uni_hal_rcc_css_callback(void) {
}

void NMI_Handler(void) {
    if (LL_RCC_IsActiveFlag_HSECSS()) {
        LL_RCC_ClearFlag_HSECSS();
        if (rcc_init_in_progress) {
            css_during_init = true;
            g_uni_hal_rcc_status.hse_inited = false;
            g_uni_hal_rcc_status.pll_inited = false;
            g_uni_hal_rcc_status.sys_inited = false;
            g_uni_hal_rcc_status.inited = false;
            // Init owns the in-flight HCLK/reload pair and will finish rescaling it.
        } else if (g_uni_hal_rcc_config) {
            bool was_inited = g_uni_hal_rcc_status.inited;
            g_uni_hal_rcc_status.inited = _uni_hal_rcc_recover() && was_inited;
        }
        uni_hal_rcc_css_callback();
    }
}

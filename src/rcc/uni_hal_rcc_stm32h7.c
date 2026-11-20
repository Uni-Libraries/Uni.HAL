//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// cmsis
#include <stm32h7xx.h>

// st
#include <stm32h7xx_ll_cortex.h>
#include <stm32h7xx_ll_rcc.h>
#include <stm32h7xx_ll_system.h>
#include <stm32h7xx_ll_utils.h>

// Uni.Common
#include <uni_common.h>

// uni_hal
#include "dwt/uni_hal_dwt.h"
#include "flash/uni_hal_flash.h"
#include "gpio/uni_hal_gpio.h"
#include "pwr/uni_hal_pwr_stm32.h"
#include "rcc/uni_hal_rcc.h"
#include "rcc/uni_hal_rcc_rtc_stm32_internal.h"
#include "rcc/uni_hal_rcc_stm32h7.h"
#include "rcc/uni_hal_rcc_stm32h7_internal.h"
#include "systick/uni_hal_systick.h"
#include "systick/uni_hal_systick_stm32.h"



//
// Globals
//

static const uint32_t eth1_clock_mask = LL_AHB1_GRP1_PERIPH_ETH1MAC | LL_AHB1_GRP1_PERIPH_ETH1RX | LL_AHB1_GRP1_PERIPH_ETH1TX;

static uni_hal_rcc_stm32h7_config_t rcc_configs[2];
static const uni_hal_rcc_stm32h7_config_t *volatile g_uni_hal_rcc_config;
static volatile uni_hal_rcc_stm32h7_status_t g_uni_hal_rcc_status;



//
// Private
//


typedef struct {
    uint32_t rcc_cr;
    uint32_t rcc_cfgr;
    uint32_t rcc_pllckselr;
    uint32_t rcc_pllcfgr;
    uint32_t rcc_pll1divr;
    uint32_t rcc_pll2divr;
    uint32_t rcc_pll3divr;
    uint32_t rcc_cifr;
    uint32_t rcc_rsr;
    uint32_t pwr_csr1;
    uint32_t pwr_d3cr;
    uint32_t flash_acr;
    uint32_t systick_ms;
} uni_hal_rcc_stm32h7_diag_t;


static volatile uni_hal_rcc_stm32h7_diag_t g_uni_hal_rcc_diag_boot = { 0 };
static volatile uint32_t g_uni_hal_rcc_reset_flags_shadow = 0U;
static volatile bool g_uni_hal_rcc_reconfig_in_progress;
static volatile bool css_recovery_active;
static volatile uint32_t css_event_count;
// Maximum SYSCLK prepared by init at the programmed bus prescalers and Flash latency.
static volatile uint32_t prepared_sysclk;


typedef enum {
    UNI_HAL_RCC_BOOT_STAGE_NONE = 0,
    UNI_HAL_RCC_BOOT_STAGE_FLASH,
    UNI_HAL_RCC_BOOT_STAGE_HSE,
    UNI_HAL_RCC_BOOT_STAGE_CSI,
    UNI_HAL_RCC_BOOT_STAGE_HSI,
    UNI_HAL_RCC_BOOT_STAGE_PLL,
    UNI_HAL_RCC_BOOT_STAGE_SYSCLK,
    UNI_HAL_RCC_BOOT_STAGE_CSS_RECOVERY
} uni_hal_rcc_boot_stage_e;


static volatile uni_hal_rcc_boot_stage_e g_uni_hal_rcc_boot_stage = UNI_HAL_RCC_BOOT_STAGE_NONE;


enum {
    UNI_HAL_RCC_NMI_WAIT_ITERATIONS = 4096U,
};


typedef struct {
    bool dwt_ready;
    uint32_t start_tick;
    uint32_t timeout_tick;
    uint32_t start_ms;
    uint32_t timeout_ms;
} uni_hal_rcc_wait_timeout_t;


static void _uni_hal_stm_rcc_diag_capture(volatile uni_hal_rcc_stm32h7_diag_t* dst) {
    if (dst == nullptr) {
        return;
    }

    dst->rcc_cr = RCC->CR;
    dst->rcc_cfgr = RCC->CFGR;
    dst->rcc_pllckselr = RCC->PLLCKSELR;
    dst->rcc_pllcfgr = RCC->PLLCFGR;
    dst->rcc_pll1divr = RCC->PLL1DIVR;
    dst->rcc_pll2divr = RCC->PLL2DIVR;
    dst->rcc_pll3divr = RCC->PLL3DIVR;
    dst->rcc_cifr = RCC->CIFR;
    uint32_t reset_flags = g_uni_hal_rcc_reset_flags_shadow;
    if (reset_flags == 0U) {
        reset_flags = RCC->RSR;
    }
    dst->rcc_rsr = reset_flags;
    dst->pwr_csr1 = PWR->CSR1;
    dst->pwr_d3cr = PWR->D3CR;
    dst->flash_acr = FLASH->ACR;
    dst->systick_ms = uni_hal_systick_get_ms();
}


static void _uni_hal_stm_rcc_wait_timeout_begin(uni_hal_rcc_wait_timeout_t* timeout_ctx, uint32_t timeout_ms) {
    if (timeout_ctx == nullptr) {
        return;
    }

    timeout_ctx->dwt_ready = uni_hal_dwt_is_inited();
    timeout_ctx->start_tick = 0U;
    timeout_ctx->timeout_tick = 0U;
    timeout_ctx->start_ms = uni_hal_systick_get_ms();
    timeout_ctx->timeout_ms = timeout_ms;

    if (timeout_ctx->dwt_ready) {
        timeout_ctx->start_tick = uni_hal_dwt_get_tick();
        timeout_ctx->timeout_tick = uni_hal_dwt_timeout_ms_to_tick(timeout_ms);
    }
}


static bool _uni_hal_stm_rcc_wait_timeout_expired(const uni_hal_rcc_wait_timeout_t* timeout_ctx) {
    if (timeout_ctx == nullptr) {
        return true;
    }

    if (timeout_ctx->timeout_ms == 0U) {
        return true;
    }

    if (timeout_ctx->dwt_ready) {
        if (timeout_ctx->timeout_tick == 0U) {
            uint32_t elapsed_ms = uni_hal_systick_get_ms() - timeout_ctx->start_ms;
            return elapsed_ms >= timeout_ctx->timeout_ms;
        }

        uint32_t elapsed_tick = uni_hal_dwt_compare(timeout_ctx->start_tick, uni_hal_dwt_get_tick());
        return elapsed_tick >= timeout_ctx->timeout_tick;
    }

    uint32_t elapsed_ms = uni_hal_systick_get_ms() - timeout_ctx->start_ms;
    return elapsed_ms >= timeout_ctx->timeout_ms;
}


static bool _uni_hal_stm_rcc_wait_ready(uint32_t (*get_state)(void), uint32_t expected, uint32_t timeout_ms) {
    if (get_state() == expected) {
        return true;
    }
    if (css_recovery_active) {
        // Neither the FreeRTOS tick nor an optional DWT counter is a clock source for NMI.
        for (uint32_t i = 0; timeout_ms && i < UNI_HAL_RCC_NMI_WAIT_ITERATIONS; ++i) {
            if (get_state() == expected) {
                return true;
            }
        }
        return false;
    }

    uni_hal_rcc_wait_timeout_t timeout_ctx = { 0 };
    _uni_hal_stm_rcc_wait_timeout_begin(&timeout_ctx, timeout_ms);
    while (get_state() != expected) {
        if (g_uni_hal_rcc_status.css_recovery_pending || _uni_hal_stm_rcc_wait_timeout_expired(&timeout_ctx)) {
            return false;
        }
    }
    return true;
}


static uint32_t _uni_hal_stm_rcc_get_pll_input_range(uint32_t input_hz)
{
    uint32_t result = LL_RCC_PLLINPUTRANGE_1_2;
    if (input_hz >= 8'000'000)
    {
        result = LL_RCC_PLLINPUTRANGE_8_16;
    }
    else if (input_hz >= 4'000'000U) {
        result = LL_RCC_PLLINPUTRANGE_4_8;
    }
    else if (input_hz >= 2'000'000U) {
        result = LL_RCC_PLLINPUTRANGE_2_4;
    }
    return result;
}


static bool _uni_hal_stm_rcc_switch_sysclk(uint32_t source, uint32_t status, uint32_t timeout_ms) {
    LL_RCC_SetSysClkSource(source);
    return _uni_hal_stm_rcc_wait_ready(LL_RCC_GetSysClkSource, status, timeout_ms);
}

/**
 * Configure high speed external clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_hse(void) {
    bool result = false;

    LL_RCC_HSE_Disable();
    (void)_uni_hal_stm_rcc_wait_ready(LL_RCC_HSE_IsReady, false, g_uni_hal_rcc_config->timeout.hse);
    LL_RCC_HSE_DisableBypass();

    if (g_uni_hal_rcc_config->hse_bypass != false) {
        LL_RCC_HSE_EnableBypass();
    }

    LL_RCC_ClearFlag_HSECSS();
    LL_RCC_HSE_Enable();
    result = _uni_hal_stm_rcc_wait_ready(LL_RCC_HSE_IsReady, true, g_uni_hal_rcc_config->timeout.hse);

    g_uni_hal_rcc_status.hse_inited = result;

    if (result) {
        if (g_uni_hal_rcc_config->hse_css) {
            LL_RCC_ClearFlag_HSECSS();
            LL_RCC_HSE_EnableCSS();
        }
    }
    else {
        LL_RCC_HSE_Disable();
        (void)_uni_hal_stm_rcc_wait_ready(LL_RCC_HSE_IsReady, false, g_uni_hal_rcc_config->timeout.hse);
    }

    return result;
}


static bool _uni_hal_stm_rcc_enable_and_wait(void (*enable_fn)(void),
                                              uint32_t (*is_ready_fn)(void),
                                              void (*set_trim_fn)(uint32_t),
                                              uint32_t trim_value,
                                              uint32_t timeout_ms) {
    if ((enable_fn == nullptr) || (is_ready_fn == nullptr)) {
        return false;
    }

    enable_fn();
    if (set_trim_fn != nullptr) {
        set_trim_fn(trim_value);
    }

    return _uni_hal_stm_rcc_wait_ready(is_ready_fn, true, timeout_ms);
}


static bool _uni_hal_stm_rcc_csi(void) {
    g_uni_hal_rcc_status.csi_inited = _uni_hal_stm_rcc_enable_and_wait(LL_RCC_CSI_Enable,
                                                                        LL_RCC_CSI_IsReady,
                                                                        LL_RCC_CSI_SetCalibTrimming,
                                                                        30U,
                                                                        g_uni_hal_rcc_config->timeout.csi);

    return g_uni_hal_rcc_status.csi_inited;
}


/**
 * Configure high speed internal clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_hsi(void) {
    g_uni_hal_rcc_status.hsi_inited = _uni_hal_stm_rcc_enable_and_wait(LL_RCC_HSI_Enable,
                                                                        LL_RCC_HSI_IsReady,
                                                                        LL_RCC_HSI_SetCalibTrimming,
                                                                        63U,
                                                                        g_uni_hal_rcc_config->timeout.hsi);

    return g_uni_hal_rcc_status.hsi_inited;
}



/**
 * Configure low speed external clockAHBPrescTable
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_lse(void) {
    // Check LSE_IsReady flag
    g_uni_hal_rcc_status.lse_inited = LL_RCC_LSE_IsReady();

    if (!g_uni_hal_rcc_status.lse_inited) {
        uni_hal_pwr_stm_set_backup_access(true);
        LL_RCC_LSE_SetDriveCapability(LL_RCC_LSEDRIVE_MEDIUMLOW);
        LL_RCC_LSE_Enable();

        bool lse_ready = _uni_hal_stm_rcc_wait_ready(LL_RCC_LSE_IsReady,
                                                     true,
                                                     g_uni_hal_rcc_config->timeout.lse);
        uni_hal_pwr_stm_set_backup_access(false);

        // Check LSE_IsReady flag
        g_uni_hal_rcc_status.lse_inited = lse_ready && (LL_RCC_LSE_IsReady() != 0U);

        // Try one time to reset Backup Domain and init LSE again
        if (!g_uni_hal_rcc_status.lse_inited && g_uni_hal_rcc_config->lse_backup_reset &&
            !g_uni_hal_rcc_status.lse_backup_reseted) {
            uni_hal_rcc_reset(UNI_HAL_RCC_RESET_BACKUP);
            g_uni_hal_rcc_status.lse_backup_reseted = true;
            g_uni_hal_rcc_status.lse_inited = _uni_hal_stm_rcc_lse();
        }
    }

    return g_uni_hal_rcc_status.lse_inited;
}


/**
 * Configure low speed internal clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_lsi(void) {
    g_uni_hal_rcc_status.lsi_inited = _uni_hal_stm_rcc_enable_and_wait(LL_RCC_LSI_Enable,
                                                                        LL_RCC_LSI_IsReady,
                                                                        nullptr,
                                                                        0U,
                                                                        g_uni_hal_rcc_config->timeout.lsi);

    return g_uni_hal_rcc_status.lsi_inited;
}


static bool _uni_hal_stm_rcc_pll_reset(void){
    // disable PLLs
    LL_RCC_PLL1_Disable();
    LL_RCC_PLL2_Disable();
    LL_RCC_PLL3_Disable();

    // wait for ready
    if (!_uni_hal_stm_rcc_wait_ready(LL_RCC_PLL1_IsReady, false, g_uni_hal_rcc_config->timeout.pll)) {
        return false;
    }
    if (!_uni_hal_stm_rcc_wait_ready(LL_RCC_PLL2_IsReady, false, g_uni_hal_rcc_config->timeout.pll)) {
        return false;
    }
    if (!_uni_hal_stm_rcc_wait_ready(LL_RCC_PLL3_IsReady, false, g_uni_hal_rcc_config->timeout.pll)) {
        return false;
    }

    LL_RCC_PLL1P_Disable();
    LL_RCC_PLL1Q_Disable();
    LL_RCC_PLL1R_Disable();
    LL_RCC_PLL2P_Disable();
    LL_RCC_PLL2Q_Disable();
    LL_RCC_PLL2R_Disable();
    LL_RCC_PLL3P_Disable();
    LL_RCC_PLL3Q_Disable();
    LL_RCC_PLL3R_Disable();

    LL_RCC_PLL1FRACN_Disable();
    LL_RCC_PLL2FRACN_Disable();
    LL_RCC_PLL3FRACN_Disable();

    // common
    LL_RCC_WriteReg(PLLCKSELR, RCC_PLLCKSELR_DIVM1_5 | RCC_PLLCKSELR_DIVM2_5 | RCC_PLLCKSELR_DIVM3_5);
    LL_RCC_WriteReg(PLLCFGR, 0x01FF0000U);

    // pll 1
    LL_RCC_WriteReg(PLL1DIVR, 0x01010280U);
    CLEAR_REG(RCC->PLL1FRACR);

    // pll 2
    LL_RCC_WriteReg(PLL2DIVR, 0x01010280U);
    CLEAR_REG(RCC->PLL2FRACR);

    // pll 3
    LL_RCC_WriteReg(PLL3DIVR, 0x01010280U);
    CLEAR_REG(RCC->PLL3FRACR);

    return true;
}

/**
 * Get the M divider for the clock source that actually feeds the PLLs.
 * The configured dividers are written for HSE. With HSI or CSI as the source M is scaled by
 * source/HSE, so that the PLL reference frequency and with it every PLL output stays the same.
 * @param pll_m configured M divider
 * @param source_hz frequency of the PLL clock source
 * @return M divider, 0 when the reference frequency cannot be reproduced from this source
 */
static uint32_t _uni_hal_stm_rcc_pll_get_m(uint32_t pll_m, uint32_t source_hz) {
    uint64_t const scaled = (uint64_t)pll_m * (uint64_t)source_hz;
    if ((pll_m == 0U) || ((scaled % (uint64_t)HSE_VALUE) != 0U)) {
        return 0U;
    }

    uint64_t const result = scaled / (uint64_t)HSE_VALUE;
    return (result > 63U) ? 0U : (uint32_t)result;
}

/**
 * Configure phase locked loops
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_pll(void) {
    uint32_t clock_source = LL_RCC_PLLSOURCE_HSI;
    uint32_t source_hz = HSI_VALUE;

    if (g_uni_hal_rcc_status.hse_inited) {
        clock_source = LL_RCC_PLLSOURCE_HSE;
        source_hz = HSE_VALUE;
    }
    else if (g_uni_hal_rcc_status.csi_inited) {
        clock_source = LL_RCC_PLLSOURCE_CSI;
        source_hz = CSI_VALUE;
    }
    else if (g_uni_hal_rcc_status.hsi_inited) {
        clock_source = LL_RCC_PLLSOURCE_HSI;
        source_hz = HSI_VALUE >> (LL_RCC_HSI_GetDivider() >> RCC_CR_HSIDIV_Pos);
    }
    else {
        return false;
    }

    if (!_uni_hal_stm_rcc_pll_reset()) {
        return false;
    }

    LL_RCC_PLL_SetSource(clock_source);

    bool result = true;

    // PLL1
    {
        uint32_t const pll_m = _uni_hal_stm_rcc_pll_get_m(g_uni_hal_rcc_config->pll[0].m, source_hz);
        if (pll_m == 0U) {
            g_uni_hal_rcc_status.pll_inited[0] = false;
            result = false;
        }
        else {
            uint32_t const pll_input_range = _uni_hal_stm_rcc_get_pll_input_range(source_hz / pll_m);

            if (g_uni_hal_rcc_config->pll[0].p != 0U) {
                LL_RCC_PLL1P_Enable();
                LL_RCC_PLL1_SetP(g_uni_hal_rcc_config->pll[0].p);
            }
            if (g_uni_hal_rcc_config->pll[0].q != 0U) {
                LL_RCC_PLL1Q_Enable();
                LL_RCC_PLL1_SetQ(g_uni_hal_rcc_config->pll[0].q);
            }
            if (g_uni_hal_rcc_config->pll[0].r != 0U) {
                LL_RCC_PLL1R_Enable();
                LL_RCC_PLL1_SetR(g_uni_hal_rcc_config->pll[0].r);
            }

            LL_RCC_PLL1_SetVCOInputRange(pll_input_range);
            LL_RCC_PLL1_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);
            LL_RCC_PLL1_SetM(pll_m);
            LL_RCC_PLL1_SetN(g_uni_hal_rcc_config->pll[0].n);

            if (g_uni_hal_rcc_config->pll[0].fracn != 0U) {
                LL_RCC_PLL1_SetFRACN(g_uni_hal_rcc_config->pll[0].fracn);
                LL_RCC_PLL1FRACN_Enable();
            }

            LL_RCC_PLL1_Enable();
            g_uni_hal_rcc_status.pll_inited[0] = _uni_hal_stm_rcc_wait_ready(LL_RCC_PLL1_IsReady,
                                                                              true,
                                                                              g_uni_hal_rcc_config->timeout.pll);
            result = result && g_uni_hal_rcc_status.pll_inited[0];
        }
    }

    // PLL2
    {
        bool start_pll_2 = false;
        if (g_uni_hal_rcc_config->pll[1].p != 0U) {
            LL_RCC_PLL2P_Enable();
            LL_RCC_PLL2_SetP(g_uni_hal_rcc_config->pll[1].p);
            start_pll_2 = true;
        }
        if (g_uni_hal_rcc_config->pll[1].q != 0U) {
            LL_RCC_PLL2Q_Enable();
            LL_RCC_PLL2_SetQ(g_uni_hal_rcc_config->pll[1].q);
            start_pll_2 = true;
        }
        if (g_uni_hal_rcc_config->pll[1].r != 0U) {
            LL_RCC_PLL2R_Enable();
            LL_RCC_PLL2_SetR(g_uni_hal_rcc_config->pll[1].r);
            start_pll_2 = true;
        }

        if (start_pll_2) {
            uint32_t const pll_m = _uni_hal_stm_rcc_pll_get_m(g_uni_hal_rcc_config->pll[1].m, source_hz);
            if (pll_m == 0U) {
                g_uni_hal_rcc_status.pll_inited[1] = false;
                result = false;
            }
            else {
                uint32_t const pll_input_range = _uni_hal_stm_rcc_get_pll_input_range(source_hz / pll_m);

                LL_RCC_PLL2_SetVCOInputRange(pll_input_range);
                LL_RCC_PLL2_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);
                LL_RCC_PLL2_SetM(pll_m);
                LL_RCC_PLL2_SetN(g_uni_hal_rcc_config->pll[1].n);

                if (g_uni_hal_rcc_config->pll[1].fracn != 0U) {
                    LL_RCC_PLL2_SetFRACN(g_uni_hal_rcc_config->pll[1].fracn);
                    LL_RCC_PLL2FRACN_Enable();
                }

                LL_RCC_PLL2_Enable();
                g_uni_hal_rcc_status.pll_inited[1] = _uni_hal_stm_rcc_wait_ready(LL_RCC_PLL2_IsReady,
                                                                                  true,
                                                                                  g_uni_hal_rcc_config->timeout.pll);
                result = result && g_uni_hal_rcc_status.pll_inited[1];
            }
        }
        else {
            g_uni_hal_rcc_status.pll_inited[1] = false;
        }
    }

    // PLL3
    {
        bool start_pll_3 = false;
        if (g_uni_hal_rcc_config->pll[2].p != 0U) {
            LL_RCC_PLL3P_Enable();
            LL_RCC_PLL3_SetP(g_uni_hal_rcc_config->pll[2].p);
            start_pll_3 = true;
        }
        if (g_uni_hal_rcc_config->pll[2].q != 0U) {
            LL_RCC_PLL3Q_Enable();
            LL_RCC_PLL3_SetQ(g_uni_hal_rcc_config->pll[2].q);
            start_pll_3 = true;
        }
        if (g_uni_hal_rcc_config->pll[2].r != 0U) {
            LL_RCC_PLL3R_Enable();
            LL_RCC_PLL3_SetR(g_uni_hal_rcc_config->pll[2].r);
            start_pll_3 = true;
        }

        if (start_pll_3) {
            uint32_t const pll_m = _uni_hal_stm_rcc_pll_get_m(g_uni_hal_rcc_config->pll[2].m, source_hz);
            if (pll_m == 0U) {
                g_uni_hal_rcc_status.pll_inited[2] = false;
                result = false;
            }
            else {
                uint32_t const pll_input_range = _uni_hal_stm_rcc_get_pll_input_range(source_hz / pll_m);

                LL_RCC_PLL3_SetVCOInputRange(pll_input_range);
                LL_RCC_PLL3_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);
                LL_RCC_PLL3_SetM(pll_m);
                LL_RCC_PLL3_SetN(g_uni_hal_rcc_config->pll[2].n);

                if (g_uni_hal_rcc_config->pll[2].fracn != 0U) {
                    LL_RCC_PLL3_SetFRACN(g_uni_hal_rcc_config->pll[2].fracn);
                    LL_RCC_PLL3FRACN_Enable();
                }

                LL_RCC_PLL3_Enable();
                g_uni_hal_rcc_status.pll_inited[2] = _uni_hal_stm_rcc_wait_ready(LL_RCC_PLL3_IsReady,
                                                                                  true,
                                                                                  g_uni_hal_rcc_config->timeout.pll);
                result = result && g_uni_hal_rcc_status.pll_inited[2];
            }
        }
        else {
            g_uni_hal_rcc_status.pll_inited[2] = false;
        }
    }

    if (!result) {
        LL_RCC_PLL1_Disable();
        LL_RCC_PLL2_Disable();
        LL_RCC_PLL3_Disable();

        // none of them runs any more, whatever was locked before
        g_uni_hal_rcc_status.pll_inited[0] = false;
        g_uni_hal_rcc_status.pll_inited[1] = false;
        g_uni_hal_rcc_status.pll_inited[2] = false;
    }

    return result;
}

/**
 * Configure device system clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_sysclk(void) {
    uint32_t clock_source;
    uint32_t clock_source_status;
    uint32_t target_sysclk_hz = 0U;

    if (g_uni_hal_rcc_status.pll_inited[0]) {
        LL_PLL_ClocksTypeDef pll1_clocks = { 0 };
        LL_RCC_GetPLL1ClockFreq(&pll1_clocks);

        clock_source = LL_RCC_SYS_CLKSOURCE_PLL1;
        clock_source_status = LL_RCC_SYS_CLKSOURCE_STATUS_PLL1;
        target_sysclk_hz = pll1_clocks.PLL_P_Frequency;
    } else if (g_uni_hal_rcc_status.hse_inited) {
        clock_source = LL_RCC_SYS_CLKSOURCE_HSE;
        clock_source_status = LL_RCC_SYS_CLKSOURCE_STATUS_HSE;
        target_sysclk_hz = HSE_VALUE;
    } else if (g_uni_hal_rcc_status.csi_inited) {
        clock_source = LL_RCC_SYS_CLKSOURCE_CSI;
        clock_source_status = LL_RCC_SYS_CLKSOURCE_STATUS_CSI;
        target_sysclk_hz = CSI_VALUE;
    } else if (g_uni_hal_rcc_status.hsi_inited) {
        clock_source = LL_RCC_SYS_CLKSOURCE_HSI;
        clock_source_status = LL_RCC_SYS_CLKSOURCE_STATUS_HSI;
        target_sysclk_hz = HSI_VALUE >> (LL_RCC_HSI_GetDivider() >> RCC_CR_HSIDIV_Pos);
    }
    else{
        return false;
    }

    uint32_t const sys_prescaler = LL_RCC_SYSCLK_DIV_1;
    uint32_t const ahb_prescaler = LL_RCC_AHB_DIV_2;
    uint32_t const apb1_prescaler = LL_RCC_APB1_DIV_2;
    uint32_t const apb2_prescaler = LL_RCC_APB2_DIV_2;
    uint32_t const apb3_prescaler = LL_RCC_APB3_DIV_2;
    uint32_t const apb4_prescaler = LL_RCC_APB4_DIV_2;

    if (target_sysclk_hz == 0U) {
        g_uni_hal_rcc_status.sys_inited = false;
        return false;
    }

    uint32_t const target_cpuclk_hz = LL_RCC_CALC_SYSCLK_FREQ(target_sysclk_hz, sys_prescaler);
    uint32_t const target_axi_hz = LL_RCC_CALC_HCLK_FREQ(target_sysclk_hz, ahb_prescaler);
    if ((target_cpuclk_hz == 0U) || (target_axi_hz == 0U)) {
        g_uni_hal_rcc_status.sys_inited = false;
        return false;
    }

    prepared_sysclk = 0U;
    if (!uni_hal_flash_configure_latency_auto(target_axi_hz)) {
        g_uni_hal_rcc_status.sys_inited = false;
        return false;
    }

    // set prescalers
    LL_RCC_SetSysPrescaler(sys_prescaler);
    LL_RCC_SetAHBPrescaler(ahb_prescaler);
    LL_RCC_SetAPB1Prescaler(apb1_prescaler);
    LL_RCC_SetAPB2Prescaler(apb2_prescaler);
    LL_RCC_SetAPB3Prescaler(apb3_prescaler);
    LL_RCC_SetAPB4Prescaler(apb4_prescaler);

    prepared_sysclk = target_sysclk_hz;

    // Reconfigure sysclk
    if (!_uni_hal_stm_rcc_switch_sysclk(clock_source, clock_source_status, g_uni_hal_rcc_config->timeout.pll)) {
        g_uni_hal_rcc_status.sys_inited = false;
        return false;
    }

    g_uni_hal_rcc_status.sys_inited = true;

    // Recalculate clock
    SystemCoreClockUpdate();

    return true;
}



static bool _uni_hal_rcc_css_dividers_prepared(void) {
    return LL_RCC_GetSysPrescaler() == LL_RCC_SYSCLK_DIV_1 &&
           LL_RCC_GetAHBPrescaler() == LL_RCC_AHB_DIV_2 &&
           LL_RCC_GetAPB1Prescaler() == LL_RCC_APB1_DIV_2 &&
           LL_RCC_GetAPB2Prescaler() == LL_RCC_APB2_DIV_2 &&
           LL_RCC_GetAPB3Prescaler() == LL_RCC_APB3_DIV_2 &&
           LL_RCC_GetAPB4Prescaler() == LL_RCC_APB4_DIV_2;
}

// CSS has already switched an HSE-based SYSCLK to HSI. Keep the current Flash timing
// and bus dividers; the PLL can resume only at the frequency prepared by startup.
static void _uni_hal_rcc_recover_css(void) {
    uint32_t const old_core_hz = SystemCoreClock;
    uint32_t const timeout = g_uni_hal_rcc_config->timeout.pll;
    css_recovery_active = true;
    g_uni_hal_rcc_status.inited = false;
    g_uni_hal_rcc_status.hse_inited = false;
    g_uni_hal_rcc_status.sys_inited = false;
    g_uni_hal_rcc_status.pll_inited[0] = false;
    g_uni_hal_rcc_status.pll_inited[1] = false;
    g_uni_hal_rcc_status.pll_inited[2] = false;
    g_uni_hal_rcc_status.csi_inited = LL_RCC_CSI_IsReady();
    LL_RCC_HSI_Enable();
    g_uni_hal_rcc_status.hsi_inited = _uni_hal_stm_rcc_wait_ready(LL_RCC_HSI_IsReady, 1U, g_uni_hal_rcc_config->timeout.hsi);
    if (g_uni_hal_rcc_status.hsi_inited &&
        _uni_hal_stm_rcc_switch_sysclk(LL_RCC_SYS_CLKSOURCE_HSI, LL_RCC_SYS_CLKSOURCE_STATUS_HSI, timeout)) {
        if (prepared_sysclk != 0U && _uni_hal_rcc_css_dividers_prepared() && _uni_hal_stm_rcc_pll()) {
            LL_PLL_ClocksTypeDef pll;
            LL_RCC_GetPLL1ClockFreq(&pll);
            if (pll.PLL_P_Frequency == prepared_sysclk) {
                g_uni_hal_rcc_status.inited = _uni_hal_stm_rcc_switch_sysclk(
                    LL_RCC_SYS_CLKSOURCE_PLL1, LL_RCC_SYS_CLKSOURCE_STATUS_PLL1, timeout);
            }
        }
    }
    if (!g_uni_hal_rcc_status.inited) {
        // Cancel a late PLL switch. Report fallback only after SWS confirms HSI.
        if (g_uni_hal_rcc_status.hsi_inited) {
            g_uni_hal_rcc_status.sys_inited = _uni_hal_stm_rcc_switch_sysclk(
                LL_RCC_SYS_CLKSOURCE_HSI, LL_RCC_SYS_CLKSOURCE_STATUS_HSI, timeout);
        } else {
            LL_RCC_SetSysClkSource(LL_RCC_GetSysClkSource() >> RCC_CFGR_SWS_Pos);
        }
    } else {
        g_uni_hal_rcc_status.sys_inited = true;
    }
    SystemCoreClockUpdate();
    uni_hal_systick_reclock(old_core_hz, SystemCoreClock);
    css_recovery_active = false;
}

bool uni_hal_rcc_init(void) {
    bool result = false;

    uint32_t const mask = uni_hal_core_irq_pause();
    if (g_uni_hal_rcc_config == nullptr || g_uni_hal_rcc_reconfig_in_progress) {
        uni_hal_core_irq_resume(mask);
        return false;
    }

    g_uni_hal_rcc_reconfig_in_progress = true;
    uni_hal_core_irq_resume(mask);

    if (!g_uni_hal_rcc_status.inited && !g_uni_hal_rcc_status.css_recovery_pending) {
        g_uni_hal_rcc_status.sys_inited = false;
        g_uni_hal_rcc_status.hse_inited = false;
        g_uni_hal_rcc_status.csi_inited = false;
        g_uni_hal_rcc_status.lse_inited = false;
        // Keep the reason for this boot and clear the flags: they are sticky, and left alone
        // the next boot would show its own reset on top of this one.
        g_uni_hal_rcc_reset_flags_shadow = RCC->RSR;
        LL_RCC_ClearResetFlags();

        if (!uni_hal_dwt_is_inited()) {
            (void)uni_hal_dwt_init();
        }

        result = uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_SYSCFG, true);
        (void)uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_PWR, true);
        if (!result) {
            g_uni_hal_rcc_boot_stage = UNI_HAL_RCC_BOOT_STAGE_NONE;
            _uni_hal_stm_rcc_diag_capture(&g_uni_hal_rcc_diag_boot);
            g_uni_hal_rcc_status.inited = false;
            goto finish;
        }

        SystemCoreClockUpdate();
        uni_hal_systick_init();

        g_uni_hal_rcc_boot_stage = UNI_HAL_RCC_BOOT_STAGE_HSE;
        if(g_uni_hal_rcc_config->hse_enable) {
            (void)_uni_hal_stm_rcc_hse();
        }

        g_uni_hal_rcc_boot_stage = UNI_HAL_RCC_BOOT_STAGE_CSI;
        if(g_uni_hal_rcc_config->csi_enable) {
            (void)_uni_hal_stm_rcc_csi();
        }

        g_uni_hal_rcc_boot_stage = UNI_HAL_RCC_BOOT_STAGE_HSI;
        _uni_hal_stm_rcc_hsi();

        if(g_uni_hal_rcc_config->lse_enable != false) {
            _uni_hal_stm_rcc_lse();
        }
        if (!g_uni_hal_rcc_status.lse_inited) {
            _uni_hal_stm_rcc_lsi();
        }

        g_uni_hal_rcc_boot_stage = UNI_HAL_RCC_BOOT_STAGE_PLL;
        bool pll_ok = _uni_hal_stm_rcc_pll();

        g_uni_hal_rcc_boot_stage = UNI_HAL_RCC_BOOT_STAGE_SYSCLK;
        if (pll_ok && !g_uni_hal_rcc_status.css_recovery_pending) {
            result = _uni_hal_stm_rcc_sysclk() && result;
        }

        uni_hal_systick_init();
        _uni_hal_stm_rcc_diag_capture(&g_uni_hal_rcc_diag_boot);
        g_uni_hal_rcc_boot_stage = UNI_HAL_RCC_BOOT_STAGE_NONE;

        g_uni_hal_rcc_status.inited = result && pll_ok && g_uni_hal_rcc_status.sys_inited;
    }

finish:
    for (;;) {
        if (g_uni_hal_rcc_status.css_recovery_pending) {
            g_uni_hal_rcc_status.css_recovery_pending = false;
            _uni_hal_rcc_recover_css();
        }
        // An NMI just before release belongs to this init; one just after owns its recovery.
        uint32_t const release_mask = uni_hal_core_irq_pause();
        g_uni_hal_rcc_reconfig_in_progress = false;
        if (!g_uni_hal_rcc_status.css_recovery_pending) {
            uni_hal_core_irq_resume(release_mask);
            return g_uni_hal_rcc_status.inited;
        }
        g_uni_hal_rcc_reconfig_in_progress = true;
        uni_hal_core_irq_resume(release_mask);
    }
}


bool uni_hal_rcc_is_inited(void) {
    return g_uni_hal_rcc_status.inited;
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
            // unknown reset
            break;
    }


    return result;
}


//
// Clock
//

/**
 * Get HCLK frequency
 * @return HCLK frequency in hertz
 */
uint32_t uni_hal_rcc_stm32h7_clk_get_hclk(void) {
    // get HCLK freq
    LL_RCC_ClocksTypeDef clocks;
    LL_RCC_GetSystemClocksFreq(&clocks);
    return clocks.HCLK_Frequency;
}


bool uni_hal_rcc_clk_get(uni_hal_core_periph_e clock) { //-V2008
    bool result = false;

    switch (clock) {
        case UNI_HAL_CORE_PERIPH_ADC_1:
        case UNI_HAL_CORE_PERIPH_ADC_2:
            result = LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_ADC12);
            break;
        case UNI_HAL_CORE_PERIPH_ADC_3:
            result = LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_ADC3);
            break;
        case UNI_HAL_CORE_PERIPH_CAN_1:
        case UNI_HAL_CORE_PERIPH_CAN_2:
            result = LL_APB1_GRP2_IsEnabledClock(LL_APB1_GRP2_PERIPH_FDCAN);
            break;
        case UNI_HAL_CORE_PERIPH_CRC_1:
            result = LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_CRC);
            break;
        case UNI_HAL_CORE_PERIPH_DAC_1:
        case UNI_HAL_CORE_PERIPH_DAC_2:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_DAC12);
            break;
        case UNI_HAL_CORE_PERIPH_DMA_1:
            result = LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_DMA1);
            break;
        case UNI_HAL_CORE_PERIPH_DMA_2:
            result = LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_DMA2);
            break;
        case UNI_HAL_CORE_PERIPH_ETH_1:
            result = LL_AHB1_GRP1_IsEnabledClock(eth1_clock_mask);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_A:
            result = LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_GPIOA);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_B:
            result = LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_GPIOB);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_C:
            result = LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_GPIOC);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_D:
            result = LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_GPIOD);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_E:
            result = LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_GPIOE);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_F:
            result = LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_GPIOF);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_G:
            result = LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_GPIOG);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_H:
            result = LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_GPIOH);
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_I:
            result = LL_AHB4_GRP1_IsEnabledClock(LL_AHB4_GRP1_PERIPH_GPIOI);
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
            result = LL_APB4_GRP1_IsEnabledClock(LL_APB4_GRP1_PERIPH_I2C4);
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
        case UNI_HAL_CORE_PERIPH_SPI_4:
            result = LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_SPI4);
            break;
        case UNI_HAL_CORE_PERIPH_SPI_5:
            result = LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_SPI5);
            break;
        case UNI_HAL_CORE_PERIPH_SPI_6:
            result = LL_APB4_GRP1_IsEnabledClock(LL_APB4_GRP1_PERIPH_SPI6);
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
        case UNI_HAL_CORE_PERIPH_UART_6:
            result = LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_USART6);
            break;
        case UNI_HAL_CORE_PERIPH_UART_7:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_UART7);
            break;
        case UNI_HAL_CORE_PERIPH_UART_8:
            result = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_UART8);
            break;
        case UNI_HAL_CORE_PERIPH_PWR:
            result = true;
            break;
        case UNI_HAL_CORE_PERIPH_SYSCFG:
            result = LL_APB4_GRP1_IsEnabledClock(LL_APB4_GRP1_PERIPH_SYSCFG);
            break;
        case UNI_HAL_CORE_PERIPH_TIM_9:
        case UNI_HAL_CORE_PERIPH_TIM_10:
        case UNI_HAL_CORE_PERIPH_TIM_11:
        case UNI_HAL_CORE_PERIPH_TIM_12:
        case UNI_HAL_CORE_PERIPH_TIM_13:
        case UNI_HAL_CORE_PERIPH_TIM_14:
        default:
            // unknown or not implemented periph
            break;
    }

    return result;
}


bool uni_hal_rcc_clk_set(uni_hal_core_periph_e target, bool state) { //-V2008
    bool result = false;

    // ETH can be partially clocked. A false readback still leaves gates to clear.
    if (target == UNI_HAL_CORE_PERIPH_ETH_1) {
        if (state) {
            LL_AHB1_GRP1_EnableClock(eth1_clock_mask);
        } else {
            LL_AHB1_GRP1_DisableClock(eth1_clock_mask);
        }
        return true;
    }

    result = true;
    if (state != uni_hal_rcc_clk_get(target)) {
        result = false;
        switch (target) {
            case UNI_HAL_CORE_PERIPH_ADC_1:
            case UNI_HAL_CORE_PERIPH_ADC_2:
                state ? LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_ADC12) : LL_AHB1_GRP1_DisableClock(
                        LL_AHB1_GRP1_PERIPH_ADC12);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_ADC_3:
                state ? LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_ADC3) : LL_AHB4_GRP1_DisableClock(
                        LL_AHB4_GRP1_PERIPH_ADC3);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_CAN_1:
            case UNI_HAL_CORE_PERIPH_CAN_2:
                state ? LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_FDCAN) : LL_APB1_GRP2_DisableClock(LL_APB1_GRP2_PERIPH_FDCAN);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_CRC_1:
                state ? LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_CRC) : LL_AHB4_GRP1_DisableClock(
                        LL_AHB4_GRP1_PERIPH_CRC);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_DAC_1:
            case UNI_HAL_CORE_PERIPH_DAC_2:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC12) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_DAC12);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_DMA_1:
                state ? LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1) : LL_AHB1_GRP1_DisableClock(
                        LL_AHB1_GRP1_PERIPH_DMA1);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_DMA_2:
                state ? LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2) : LL_AHB1_GRP1_DisableClock(
                        LL_AHB1_GRP1_PERIPH_DMA2);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_GPIO_A:
                state ? LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA) : LL_AHB4_GRP1_DisableClock(
                        LL_AHB4_GRP1_PERIPH_GPIOA);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_GPIO_B:
                state ? LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOB) : LL_AHB4_GRP1_DisableClock(
                        LL_AHB4_GRP1_PERIPH_GPIOB);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_GPIO_C:
                state ? LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOC) : LL_AHB4_GRP1_DisableClock(
                        LL_AHB4_GRP1_PERIPH_GPIOC);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_GPIO_D:
                state ? LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOD) : LL_AHB4_GRP1_DisableClock(
                        LL_AHB4_GRP1_PERIPH_GPIOD);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_GPIO_E:
                state ? LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOE) : LL_AHB4_GRP1_DisableClock(
                        LL_AHB4_GRP1_PERIPH_GPIOE);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_GPIO_F:
                state ? LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOF) : LL_AHB4_GRP1_DisableClock(
                        LL_AHB4_GRP1_PERIPH_GPIOF);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_GPIO_G:
                state ? LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOG) : LL_AHB4_GRP1_DisableClock(
                        LL_AHB4_GRP1_PERIPH_GPIOG);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_GPIO_H:
                state ? LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOH) : LL_AHB4_GRP1_DisableClock(
                        LL_AHB4_GRP1_PERIPH_GPIOH);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_GPIO_I:
                state ? LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOI) : LL_AHB4_GRP1_DisableClock(
                        LL_AHB4_GRP1_PERIPH_GPIOI);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_I2C_1:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_I2C1);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_I2C_2:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_I2C2);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_I2C_3:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C3) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_I2C3);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_I2C_4:
                state ? LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_I2C4) : LL_APB4_GRP1_DisableClock(
                        LL_APB4_GRP1_PERIPH_I2C4);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_RNG:
                state ? LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_RNG) : LL_AHB2_GRP1_DisableClock(
                        LL_AHB2_GRP1_PERIPH_RNG);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_RTC:
                uni_hal_pwr_stm_set_backup_access(true);
                state ? LL_RCC_EnableRTC() : LL_RCC_DisableRTC();
                uni_hal_pwr_stm_set_backup_access(false);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_SPI_1:
                state ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1) : LL_APB2_GRP1_DisableClock(
                        LL_APB2_GRP1_PERIPH_SPI1);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_SPI_2:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_SPI2);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_SPI_3:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI3) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_SPI3);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_SPI_4:
                state ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI4) : LL_APB2_GRP1_DisableClock(
                        LL_APB2_GRP1_PERIPH_SPI4);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_SPI_5:
                state ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI5) : LL_APB2_GRP1_DisableClock(
                        LL_APB2_GRP1_PERIPH_SPI5);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_SPI_6:
                state ? LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_SPI6) : LL_APB4_GRP1_DisableClock(
                        LL_APB4_GRP1_PERIPH_SPI6);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_SYSCFG:
                state ? LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_SYSCFG) : LL_APB4_GRP1_DisableClock(
                        LL_APB4_GRP1_PERIPH_SYSCFG);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_PWR:
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_1:
                state ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1) : LL_APB2_GRP1_DisableClock(
                        LL_APB2_GRP1_PERIPH_TIM1);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_2:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_TIM2);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_3:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_TIM3);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_4:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_TIM4);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_5:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM5) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_TIM5);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_6:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM6) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_TIM6);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_7:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM7) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_TIM7);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_8:
                state ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM8) : LL_APB2_GRP1_DisableClock(
                        LL_APB2_GRP1_PERIPH_TIM8);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_15:
                state ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM15) : LL_APB2_GRP1_DisableClock(
                        LL_APB2_GRP1_PERIPH_TIM15);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_16:
                state ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM16) : LL_APB2_GRP1_DisableClock(
                        LL_APB2_GRP1_PERIPH_TIM16);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_17:
                state ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM17) : LL_APB2_GRP1_DisableClock(
                        LL_APB2_GRP1_PERIPH_TIM17);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_UART_1:
                state ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1) : LL_APB2_GRP1_DisableClock(
                        LL_APB2_GRP1_PERIPH_USART1);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_UART_2:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_USART2);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_UART_3:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_USART3);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_UART_4:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART4) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_UART4);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_UART_5:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART5) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_UART5);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_UART_6:
                state ? LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART6) : LL_APB2_GRP1_DisableClock(
                        LL_APB2_GRP1_PERIPH_USART6);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_UART_7:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART7) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_UART7);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_UART_8:
                state ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART8) : LL_APB1_GRP1_DisableClock(
                        LL_APB1_GRP1_PERIPH_UART8);
                result = true;
                break;
            case UNI_HAL_CORE_PERIPH_TIM_9:
            case UNI_HAL_CORE_PERIPH_TIM_10:
            case UNI_HAL_CORE_PERIPH_TIM_11:
            case UNI_HAL_CORE_PERIPH_TIM_12:
            case UNI_HAL_CORE_PERIPH_TIM_13:
            case UNI_HAL_CORE_PERIPH_TIM_14:
            default:
                // unknown or not implemented periph
                break;
        }
    }

    return result;
}

uint32_t uni_hal_rcc_clk_get_freq(uni_hal_core_periph_e target) {
    uint32_t result = 0U;
    switch (target) {
        case UNI_HAL_CORE_PERIPH_SYSCLK: {
            // HCLK is available from uni_hal_rcc_stm32h7_clk_get_hclk()
            LL_RCC_ClocksTypeDef clocks;
            LL_RCC_GetSystemClocksFreq(&clocks);
            result = clocks.SYSCLK_Frequency;
            break;
        }
        case UNI_HAL_CORE_PERIPH_CAN_1:
        case UNI_HAL_CORE_PERIPH_CAN_2:
            result = LL_RCC_GetFDCANClockFreq(LL_RCC_FDCAN_CLKSOURCE);
            break;
        case UNI_HAL_CORE_PERIPH_I2C_1:
        case UNI_HAL_CORE_PERIPH_I2C_2:
        case UNI_HAL_CORE_PERIPH_I2C_3:
            result = LL_RCC_GetI2CClockFreq(LL_RCC_I2C123_CLKSOURCE);
            break;
        case UNI_HAL_CORE_PERIPH_I2C_4:
            result = LL_RCC_GetI2CClockFreq(LL_RCC_I2C4_CLKSOURCE);
            break;
        case UNI_HAL_CORE_PERIPH_SPI_1:
        case UNI_HAL_CORE_PERIPH_SPI_2:
        case UNI_HAL_CORE_PERIPH_SPI_3:
            result = LL_RCC_GetSPIClockFreq(LL_RCC_SPI123_CLKSOURCE);
            break;
        case UNI_HAL_CORE_PERIPH_SPI_4:
        case UNI_HAL_CORE_PERIPH_SPI_5:
            result = LL_RCC_GetSPIClockFreq(LL_RCC_SPI45_CLKSOURCE);
            break;
        case UNI_HAL_CORE_PERIPH_SPI_6:
            result = LL_RCC_GetSPIClockFreq(LL_RCC_SPI6_CLKSOURCE);
            break;
        case UNI_HAL_CORE_PERIPH_UART_1:
        case UNI_HAL_CORE_PERIPH_UART_6:
            result = LL_RCC_GetUSARTClockFreq(LL_RCC_USART16_CLKSOURCE);
            break;
        case UNI_HAL_CORE_PERIPH_UART_2:
        case UNI_HAL_CORE_PERIPH_UART_3:
        case UNI_HAL_CORE_PERIPH_UART_4:
        case UNI_HAL_CORE_PERIPH_UART_5:
        case UNI_HAL_CORE_PERIPH_UART_7:
        case UNI_HAL_CORE_PERIPH_UART_8:
            result = LL_RCC_GetUSARTClockFreq(LL_RCC_USART234578_CLKSOURCE);
            break;
        default:
            // unknown or not implemented periph
            break;
    }

    return result;
}


//
// Clock Source
//

typedef struct {
    uni_hal_rcc_clksrc_e source;
    uint32_t selector;
} kernel_clock_source_t;

static const kernel_clock_source_t i2c_clock_sources[2][4] = {
    {
        {UNI_HAL_RCC_CLKSRC_PCLK1, LL_RCC_I2C123_CLKSOURCE_PCLK1},
        {UNI_HAL_RCC_CLKSRC_PLL3R, LL_RCC_I2C123_CLKSOURCE_PLL3R},
        {UNI_HAL_RCC_CLKSRC_HSI, LL_RCC_I2C123_CLKSOURCE_HSI},
        {UNI_HAL_RCC_CLKSRC_CSI, LL_RCC_I2C123_CLKSOURCE_CSI},
    },
    {
        {UNI_HAL_RCC_CLKSRC_PCLK4, LL_RCC_I2C4_CLKSOURCE_PCLK4},
        {UNI_HAL_RCC_CLKSRC_PLL3R, LL_RCC_I2C4_CLKSOURCE_PLL3R},
        {UNI_HAL_RCC_CLKSRC_HSI, LL_RCC_I2C4_CLKSOURCE_HSI},
        {UNI_HAL_RCC_CLKSRC_CSI, LL_RCC_I2C4_CLKSOURCE_CSI},
    },
};

static const kernel_clock_source_t spi45_clock_sources[] = {
    {UNI_HAL_RCC_CLKSRC_PCLK2, LL_RCC_SPI45_CLKSOURCE_PCLK2},
    {UNI_HAL_RCC_CLKSRC_PLL2Q, LL_RCC_SPI45_CLKSOURCE_PLL2Q},
    {UNI_HAL_RCC_CLKSRC_PLL3Q, LL_RCC_SPI45_CLKSOURCE_PLL3Q},
    {UNI_HAL_RCC_CLKSRC_HSI, LL_RCC_SPI45_CLKSOURCE_HSI},
    {UNI_HAL_RCC_CLKSRC_CSI, LL_RCC_SPI45_CLKSOURCE_CSI},
    {UNI_HAL_RCC_CLKSRC_HSE, LL_RCC_SPI45_CLKSOURCE_HSE},
};

static const kernel_clock_source_t uart_clock_sources[2][6] = {
    {
        {UNI_HAL_RCC_CLKSRC_PCLK1, LL_RCC_USART234578_CLKSOURCE_PCLK1},
        {UNI_HAL_RCC_CLKSRC_PLL2Q, LL_RCC_USART234578_CLKSOURCE_PLL2Q},
        {UNI_HAL_RCC_CLKSRC_PLL3Q, LL_RCC_USART234578_CLKSOURCE_PLL3Q},
        {UNI_HAL_RCC_CLKSRC_HSI, LL_RCC_USART234578_CLKSOURCE_HSI},
        {UNI_HAL_RCC_CLKSRC_CSI, LL_RCC_USART234578_CLKSOURCE_CSI},
        {UNI_HAL_RCC_CLKSRC_LSE, LL_RCC_USART234578_CLKSOURCE_LSE},
    },
    {
        {UNI_HAL_RCC_CLKSRC_PCLK2, LL_RCC_USART16_CLKSOURCE_PCLK2},
        {UNI_HAL_RCC_CLKSRC_PLL2Q, LL_RCC_USART16_CLKSOURCE_PLL2Q},
        {UNI_HAL_RCC_CLKSRC_PLL3Q, LL_RCC_USART16_CLKSOURCE_PLL3Q},
        {UNI_HAL_RCC_CLKSRC_HSI, LL_RCC_USART16_CLKSOURCE_HSI},
        {UNI_HAL_RCC_CLKSRC_CSI, LL_RCC_USART16_CLKSOURCE_CSI},
        {UNI_HAL_RCC_CLKSRC_LSE, LL_RCC_USART16_CLKSOURCE_LSE},
    },
};

uni_hal_rcc_clksrc_e uni_hal_rcc_clksrc_get(uni_hal_core_periph_e target) {
    uni_hal_rcc_clksrc_e result = UNI_HAL_RCC_CLKSRC_UNKNOWN;
    if (uni_hal_rcc_is_inited()) {
        switch (target) {
        case UNI_HAL_CORE_PERIPH_UART_1:
        case UNI_HAL_CORE_PERIPH_UART_2:
        case UNI_HAL_CORE_PERIPH_UART_3:
        case UNI_HAL_CORE_PERIPH_UART_4:
        case UNI_HAL_CORE_PERIPH_UART_5:
        case UNI_HAL_CORE_PERIPH_UART_6:
        case UNI_HAL_CORE_PERIPH_UART_7:
        case UNI_HAL_CORE_PERIPH_UART_8: {
            bool const apb2 = uart_uses_apb2(target);
            uint32_t const mux = LL_RCC_GetUSARTClockSource(apb2 ? LL_RCC_USART16_CLKSOURCE : LL_RCC_USART234578_CLKSOURCE);
            for (unsigned i = 0; i < 6U; ++i) {
                if (uart_clock_sources[apb2][i].selector == mux) {
                    return uart_clock_sources[apb2][i].source;
                }
            }
            break;
        }
        case UNI_HAL_CORE_PERIPH_CAN_1:
        case UNI_HAL_CORE_PERIPH_CAN_2:
            switch (LL_RCC_GetFDCANClockSource(LL_RCC_FDCAN_CLKSOURCE)) {
            case LL_RCC_FDCAN_CLKSOURCE_HSE:
                return UNI_HAL_RCC_CLKSRC_HSE;
            case LL_RCC_FDCAN_CLKSOURCE_PLL1Q:
                return UNI_HAL_RCC_CLKSRC_PLL1Q;
            case LL_RCC_FDCAN_CLKSOURCE_PLL2Q:
                return UNI_HAL_RCC_CLKSRC_PLL2Q;
            default:
                return UNI_HAL_RCC_CLKSRC_UNKNOWN;
            }
        case UNI_HAL_CORE_PERIPH_I2C_1:
        case UNI_HAL_CORE_PERIPH_I2C_2:
        case UNI_HAL_CORE_PERIPH_I2C_3:
        case UNI_HAL_CORE_PERIPH_I2C_4: {
            bool const i2c4 = target == UNI_HAL_CORE_PERIPH_I2C_4;
            uint32_t const mux = LL_RCC_GetI2CClockSource(i2c4 ? LL_RCC_I2C4_CLKSOURCE : LL_RCC_I2C123_CLKSOURCE);
            for (unsigned i = 0; i < 4U; ++i) {
                if (i2c_clock_sources[i2c4][i].selector == mux) {
                    return i2c_clock_sources[i2c4][i].source;
                }
            }
            break;
        }
        case UNI_HAL_CORE_PERIPH_RNG:
            switch(LL_RCC_GetRNGClockSource(LL_RCC_RNG_CLKSOURCE)) {
                case LL_RCC_RNG_CLKSOURCE_HSI48:
                    result = UNI_HAL_RCC_CLKSRC_HSI48;
                    break;
                case LL_RCC_RNG_CLKSOURCE_PLL1Q:
                    result = UNI_HAL_RCC_CLKSRC_PLL1Q;
                    break;
                case LL_RCC_RNG_CLKSOURCE_LSE:
                    result = UNI_HAL_RCC_CLKSRC_LSE;
                    break;
                case LL_RCC_RNG_CLKSOURCE_LSI:
                    result = UNI_HAL_RCC_CLKSRC_LSI;
                    break;
                default:
                    // unknown clock source
                    break;
            }
            break;
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
            case LL_RCC_RTC_CLKSOURCE_HSE:
                result = UNI_HAL_RCC_CLKSRC_HSE;
                break;
            default:
                // unknown clock source
                break;
            }
            break;
        case UNI_HAL_CORE_PERIPH_SPI_4:
        case UNI_HAL_CORE_PERIPH_SPI_5: {
            uint32_t const mux = LL_RCC_GetSPIClockSource(LL_RCC_SPI45_CLKSOURCE);
            for (size_t i = 0; i < sizeof spi45_clock_sources / sizeof spi45_clock_sources[0]; ++i) {
                if (spi45_clock_sources[i].selector == mux) {
                    return spi45_clock_sources[i].source;
                }
            }
            break;
        }
        case UNI_HAL_CORE_PERIPH_SPI_1:
        case UNI_HAL_CORE_PERIPH_SPI_2:
        case UNI_HAL_CORE_PERIPH_SPI_3:
            switch(LL_RCC_GetSPIClockSource(LL_RCC_SPI123_CLKSOURCE)){
                case LL_RCC_SPI123_CLKSOURCE_PLL1Q:
                    result = UNI_HAL_RCC_CLKSRC_PLL1Q;
                    break;
                case LL_RCC_SPI123_CLKSOURCE_PLL2P:
                    result = UNI_HAL_RCC_CLKSRC_PLL2P;
                    break;
                case LL_RCC_SPI123_CLKSOURCE_PLL3P:
                    result = UNI_HAL_RCC_CLKSRC_PLL3P;
                    break;
                case LL_RCC_SPI123_CLKSOURCE_I2S_CKIN:
                    result = UNI_HAL_RCC_CLKSRC_I2S_CKIN;
                    break;
                case LL_RCC_SPI123_CLKSOURCE_CLKP:
                    result = UNI_HAL_RCC_CLKSRC_CLKP;
                    break;
                default:
                    // unknown clock source
                    break;
            }
            break;
        default:
            // unknown periph
            break;
        }
    }
    return result;
}


bool uni_hal_rcc_clksrc_set(uni_hal_core_periph_e target, uni_hal_rcc_clksrc_e source) {
    bool result = false;

    if (uni_hal_rcc_is_inited()) {
        switch (target) {
            case UNI_HAL_CORE_PERIPH_ADC_1:
            case UNI_HAL_CORE_PERIPH_ADC_2:
            case UNI_HAL_CORE_PERIPH_ADC_3: {
                result = true;
                uint32_t src = 0;
                switch (source) {
                    case UNI_HAL_RCC_CLKSRC_PLL2P:
                        src = LL_RCC_ADC_CLKSOURCE_PLL2P;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL3R:
                        src = LL_RCC_ADC_CLKSOURCE_PLL3R;
                        break;
                    case UNI_HAL_RCC_CLKSRC_CLKP:
                        src = LL_RCC_ADC_CLKSOURCE_CLKP;
                        break;
                    default:
                        result = false;
                }
                if (result) {
                    LL_RCC_SetADCClockSource(src);
                }
                break;
            }
            case UNI_HAL_CORE_PERIPH_I2C_1:
            case UNI_HAL_CORE_PERIPH_I2C_2:
            case UNI_HAL_CORE_PERIPH_I2C_3:
            case UNI_HAL_CORE_PERIPH_I2C_4: {
                const kernel_clock_source_t *choices = i2c_clock_sources[target == UNI_HAL_CORE_PERIPH_I2C_4];
                for (unsigned i = 0; i < 4U; ++i) {
                    if (choices[i].source == source) {
                        LL_RCC_SetI2CClockSource(choices[i].selector);
                        return true;
                    }
                }
                break;
            }
            case UNI_HAL_CORE_PERIPH_CAN_1:
            case UNI_HAL_CORE_PERIPH_CAN_2: {
                // one kernel clock for both FDCAN instances
                result = true;
                uint32_t src = 0;
                switch (source) {
                    case UNI_HAL_RCC_CLKSRC_HSE:
                        src = LL_RCC_FDCAN_CLKSOURCE_HSE;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL1Q:
                        src = LL_RCC_FDCAN_CLKSOURCE_PLL1Q;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL2Q:
                        src = LL_RCC_FDCAN_CLKSOURCE_PLL2Q;
                        break;
                    default:
                        result = false;
                        break;
                }
                if (result) {
                    LL_RCC_SetFDCANClockSource(src);
                }
                break;
            }
            case UNI_HAL_CORE_PERIPH_RNG: {
                result = true;
                uint32_t src = 0;
                switch (source) {
                    case UNI_HAL_RCC_CLKSRC_HSI48:
                        src = LL_RCC_RNG_CLKSOURCE_HSI48;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL1Q:
                        src = LL_RCC_RNG_CLKSOURCE_PLL1Q;
                        break;
                    case UNI_HAL_RCC_CLKSRC_LSE:
                        src = LL_RCC_RNG_CLKSOURCE_LSE;
                        break;
                    case UNI_HAL_RCC_CLKSRC_LSI:
                        src = LL_RCC_RNG_CLKSOURCE_LSI;
                        break;
                    default:
                        result = false;
                        break;
                }
                if (result) {
                    LL_RCC_SetRNGClockSource(src);
                }
                break;
            }
            case UNI_HAL_CORE_PERIPH_RTC:
                return select_rtc_source(source);
            case UNI_HAL_CORE_PERIPH_SPI_1:
            case UNI_HAL_CORE_PERIPH_SPI_2:
            case UNI_HAL_CORE_PERIPH_SPI_3: {
                uint32_t src = UINT32_MAX;
                switch (source) {
                    case UNI_HAL_RCC_CLKSRC_PLL1Q:
                        src = LL_RCC_SPI123_CLKSOURCE_PLL1Q;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL2P:
                        src = LL_RCC_SPI123_CLKSOURCE_PLL2P;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL3P:
                        src = LL_RCC_SPI123_CLKSOURCE_PLL3P;
                        break;
                    case UNI_HAL_RCC_CLKSRC_I2S_CKIN:
                        src = LL_RCC_SPI123_CLKSOURCE_I2S_CKIN;
                        break;
                    case UNI_HAL_RCC_CLKSRC_CLKP:
                        src = LL_RCC_SPI123_CLKSOURCE_CLKP;
                        break;
                    default:
                        // unknown clock source
                        break;
                }
                if (src != UINT32_MAX) {
                    LL_RCC_SetSPIClockSource(src);
                    result = true;
                }
                break;
            }
            case UNI_HAL_CORE_PERIPH_SPI_4:
            case UNI_HAL_CORE_PERIPH_SPI_5:
                for (size_t i = 0; i < sizeof spi45_clock_sources / sizeof spi45_clock_sources[0]; ++i) {
                    if (spi45_clock_sources[i].source == source) {
                        LL_RCC_SetSPIClockSource(spi45_clock_sources[i].selector);
                        return true;
                    }
                }
                break;
            case UNI_HAL_CORE_PERIPH_SPI_6:{
                uint32_t src = UINT32_MAX;
                switch (source) {
                    case UNI_HAL_RCC_CLKSRC_PCLK4:
                        src = LL_RCC_SPI6_CLKSOURCE_PCLK4;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL2Q:
                        src = LL_RCC_SPI6_CLKSOURCE_PLL2Q;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL3Q:
                        src = LL_RCC_SPI6_CLKSOURCE_PLL3Q;
                        break;
                    case UNI_HAL_RCC_CLKSRC_HSI:
                        src = LL_RCC_SPI6_CLKSOURCE_HSI;
                        break;
                    case UNI_HAL_RCC_CLKSRC_CSI:
                        src = LL_RCC_SPI6_CLKSOURCE_CSI;
                        break;
                    case UNI_HAL_RCC_CLKSRC_HSE:
                        src = LL_RCC_SPI6_CLKSOURCE_HSE;
                        break;
                    default:
                        // unknown clock source
                        break;
                }
                if (src != UINT32_MAX) {
                    LL_RCC_SetSPIClockSource(src);
                    result = true;
                }
                break;
            }
            case UNI_HAL_CORE_PERIPH_UART_1:
            case UNI_HAL_CORE_PERIPH_UART_6:
            case UNI_HAL_CORE_PERIPH_UART_2:
            case UNI_HAL_CORE_PERIPH_UART_3:
            case UNI_HAL_CORE_PERIPH_UART_4:
            case UNI_HAL_CORE_PERIPH_UART_5:
            case UNI_HAL_CORE_PERIPH_UART_7:
            case UNI_HAL_CORE_PERIPH_UART_8: {
                const kernel_clock_source_t *choices = uart_clock_sources[uart_uses_apb2(target)];
                for (unsigned i = 0; i < 6U; ++i) {
                    if (choices[i].source == source) {
                        LL_RCC_SetUSARTClockSource(choices[i].selector);
                        return true;
                    }
                }
                break;
            }
            default:
                // unknown periph
                break;
        }
    }

    return result;
}


//
// STM32H7 - specific
//

bool uni_hal_rcc_stm32h7_config_set(const uni_hal_rcc_stm32h7_config_t *config) {
    if (config == nullptr) {
        return false;
    }

    uint32_t const mask = uni_hal_core_irq_pause();
    if (g_uni_hal_rcc_reconfig_in_progress) {
        uni_hal_core_irq_resume(mask);
        return false;
    }
    // An NMI can interrupt the copy. Publish only the completed snapshot.
    uni_hal_rcc_stm32h7_config_t *next = g_uni_hal_rcc_config == &rcc_configs[0] ? &rcc_configs[1] : &rcc_configs[0];
    *next = *config;
    __DMB();
    g_uni_hal_rcc_config = next;
    uni_hal_core_irq_resume(mask);
    return true;
}

uint32_t uni_hal_rcc_stm32h7_reset_flags_get(void) {
    return g_uni_hal_rcc_reset_flags_shadow;
}

uni_hal_rcc_stm32h7_status_t uni_hal_rcc_stm32h7_status_get(void) {
    uint32_t const mask = uni_hal_core_irq_pause();
    uni_hal_rcc_stm32h7_status_t status;
    uint32_t event_count;
    do {
        event_count = css_event_count;
        status = g_uni_hal_rcc_status;
    } while (event_count != css_event_count); // NMI interrupted the multiword snapshot.
    uni_hal_core_irq_resume(mask);
    return status;
}


uint32_t uni_hal_rcc_stm32_mco_enable(uint32_t mco_index, uni_hal_rcc_clksrc_e clock_source, uint32_t clock_divider) {
    uint32_t result = 0U;
    if (mco_index < 2 && clock_divider >= 1 && clock_divider <= 15) {
        uint32_t source = UINT32_MAX;
        uint32_t prescaler = UINT32_MAX;
        switch (clock_source) {
            case UNI_HAL_RCC_CLKSRC_HSI:
                source = mco_index == 0 ? LL_RCC_MCO1SOURCE_HSI : UINT32_MAX;
                break;
            case UNI_HAL_RCC_CLKSRC_LSE:
                source = mco_index == 0 ? LL_RCC_MCO1SOURCE_LSE : UINT32_MAX;
                break;
            case UNI_HAL_RCC_CLKSRC_HSE:
                source = mco_index == 0 ? LL_RCC_MCO1SOURCE_HSE : LL_RCC_MCO2SOURCE_HSE;
                break;
            case UNI_HAL_RCC_CLKSRC_PLL1Q:
                source = mco_index == 0 ? LL_RCC_MCO1SOURCE_PLL1QCLK : UINT32_MAX;
                break;
            case UNI_HAL_RCC_CLKSRC_HSI48:
                source = mco_index == 0 ? LL_RCC_MCO1SOURCE_HSI48 : UINT32_MAX;
                break;
            case UNI_HAL_RCC_CLKSRC_SYSCLK:
                source = mco_index == 0 ? UINT32_MAX : LL_RCC_MCO2SOURCE_SYSCLK;
                break;
            case UNI_HAL_RCC_CLKSRC_PLL1P:
                source = mco_index == 0 ? UINT32_MAX : LL_RCC_MCO2SOURCE_PLL1PCLK;
                break;
            case UNI_HAL_RCC_CLKSRC_PLL2P:
                source = mco_index == 0 ? UINT32_MAX : LL_RCC_MCO2SOURCE_PLL2PCLK;
                break;
            case UNI_HAL_RCC_CLKSRC_CSI:
                source = mco_index == 0 ? UINT32_MAX : LL_RCC_MCO2SOURCE_CSI;
                break;
            case UNI_HAL_RCC_CLKSRC_LSI:
                source = mco_index == 0 ? UINT32_MAX : LL_RCC_MCO2SOURCE_LSI;
                break;
            default:
                break;
        }

        switch (clock_divider) {
            case 1:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_1 : LL_RCC_MCO2_DIV_1;
                break;
            case 2:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_2 : LL_RCC_MCO2_DIV_2;
                break;
            case 3:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_3 : LL_RCC_MCO2_DIV_3;
                break;
            case 4:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_4 : LL_RCC_MCO2_DIV_4;
                break;
            case 5:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_5 : LL_RCC_MCO2_DIV_5;
                break;
            case 6:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_6 : LL_RCC_MCO2_DIV_6;
                break;
            case 7:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_7 : LL_RCC_MCO2_DIV_7;
                break;
            case 8:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_8 : LL_RCC_MCO2_DIV_8;
                break;
            case 9:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_9 : LL_RCC_MCO2_DIV_9;
                break;
            case 10:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_10 : LL_RCC_MCO2_DIV_10;
                break;
            case 11:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_11 : LL_RCC_MCO2_DIV_11;
                break;
            case 12:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_12 : LL_RCC_MCO2_DIV_12;
                break;
            case 13:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_13 : LL_RCC_MCO2_DIV_13;
                break;
            case 14:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_14 : LL_RCC_MCO2_DIV_14;
                break;
            case 15:
                prescaler = mco_index == 0 ? LL_RCC_MCO1_DIV_15 : LL_RCC_MCO2_DIV_15;
                break;
            default:
                break;
        }

        if (source == UINT32_MAX || prescaler == UINT32_MAX) {
            return 0U;
        }

        uni_hal_gpio_pin_context_t pin = {
            .gpio_bank = mco_index == 0 ? UNI_HAL_CORE_PERIPH_GPIO_A : UNI_HAL_CORE_PERIPH_GPIO_C,
            .gpio_pin = mco_index == 0 ? UNI_HAL_GPIO_PIN_8 : UNI_HAL_GPIO_PIN_9,
            .gpio_speed = UNI_HAL_GPIO_SPEED_3,
            .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
            .alternate = UNI_HAL_GPIO_ALTERNATE_0,
        };
        if (!uni_hal_gpio_pin_init(&pin)) {
            return 0U;
        }

        LL_RCC_ConfigMCO(source, prescaler);
        result = 1U;
    }
    return result;
}



//
// STM32H7 - Interrupts
//

UNI_COMMON_COMPILER_WEAK void uni_hal_rcc_css_callback(void) {
}

void NMI_Handler(void) {
    if (LL_RCC_IsActiveFlag_HSECSS()) {
        LL_RCC_ClearFlag_HSECSS();

        g_uni_hal_rcc_status.css_fault = true;
        if (g_uni_hal_rcc_reconfig_in_progress) {
            g_uni_hal_rcc_status.css_recovery_pending = true;
            g_uni_hal_rcc_status.inited = false;
            g_uni_hal_rcc_status.hse_inited = false;
            g_uni_hal_rcc_status.pll_inited[0] = false;
            g_uni_hal_rcc_status.pll_inited[1] = false;
            g_uni_hal_rcc_status.pll_inited[2] = false;
            g_uni_hal_rcc_status.sys_inited = false;
        } else if (g_uni_hal_rcc_config != nullptr) {
            _uni_hal_rcc_recover_css();
        }

        ++css_event_count;
        uni_hal_rcc_css_callback();
    }
}

//
// Includes
//

// stdlib
#include <stdint.h>
#include <string.h>

// st
#include <stm32h7xx_ll_cortex.h>
#include <stm32h7xx_ll_rcc.h>
#include <stm32h7xx_ll_system.h>
#include <stm32h7xx_ll_utils.h>

// Uni.Common
#include <uni_common.h>

// uni_hal
#include "gpio/uni_hal_gpio.h"
#include "pwr/uni_hal_pwr_stm32.h"
#include "rcc/uni_hal_rcc.h"
#include "rcc/uni_hal_rcc_stm32h7.h"
#include "systick/uni_hal_systick.h"



//
// Globals
//

uni_hal_rcc_stm32h7_config_t* g_uni_hal_rcc_config = nullptr;
uni_hal_rcc_stm32h7_status_t  g_uni_hal_rcc_status = { 0 };



//
// Private
//


static void _uni_hal_stm_rcc_reset(void){
    memset(&g_uni_hal_rcc_status, 0, sizeof(g_uni_hal_rcc_status));
}

/**
 * Configure FLASH latency
 * TODO: move out
 */
static void _uni_hal_stm_rcc_flash(void) {
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2) {
    }
}


/**
 * Configure high speed external clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_hse(void) {
    // Enable HSE
    if(g_uni_hal_rcc_config->hse_bypass != false) {
        LL_RCC_HSE_EnableBypass();
    }
    LL_RCC_HSE_Enable();

    // Wait till HSE is ready
    uint32_t ticks = uni_hal_systick_get_ms();
    while (!LL_RCC_HSE_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.hse) {
    }

    // save HSE status
    g_uni_hal_rcc_status.hse_inited = LL_RCC_HSE_IsReady();

    // enable CSS
    if (g_uni_hal_rcc_status.hse_inited && g_uni_hal_rcc_config->hse_css) {
        LL_RCC_HSE_EnableCSS();
    }
    else {
        LL_RCC_HSE_Disable();
    }

    return g_uni_hal_rcc_status.hse_inited;
}


static bool _uni_hal_stm_rcc_csi(void) {
    LL_RCC_CSI_Enable();

    // Wait till HSI is ready
    uint32_t ticks = uni_hal_systick_get_ms();
    while (!LL_RCC_CSI_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.csi) {
    }
    g_uni_hal_rcc_status.csi_inited = LL_RCC_CSI_IsReady();

    return g_uni_hal_rcc_status.csi_inited;
}


/**
 * Configure high speed internal clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_hsi(void) {
    LL_RCC_HSI_Enable();

    // Wait till HSI is ready
    uint32_t ticks = uni_hal_systick_get_ms();
    while (!LL_RCC_HSI_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.hsi) {
    }
    g_uni_hal_rcc_status.hsi_inited = LL_RCC_HSI_IsReady();

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

        /* Wait till LSE is ready */
        uint32_t ticks = uni_hal_systick_get_ms();
        while (!LL_RCC_LSE_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.lse) {
        }
        uni_hal_pwr_stm_set_backup_access(false);

        // Check LSE_IsReady flag
        g_uni_hal_rcc_status.lse_inited = LL_RCC_LSE_IsReady();

        // Try one time to reset Backup Domain and init LSE again
        if (!g_uni_hal_rcc_status.lse_inited && !g_uni_hal_rcc_status.lse_backup_reseted) {
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
    LL_RCC_LSI_Enable();

    // Wait till LSE is ready
    uint32_t ticks = uni_hal_systick_get_ms();
    while (!LL_RCC_LSI_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.lsi) {
    }

    g_uni_hal_rcc_status.lsi_inited = LL_RCC_LSI_IsReady();

    return g_uni_hal_rcc_status.lsi_inited;
}


static void _uni_hal_stm_rcc_pll_reset(void){
    // disable PLLs
    LL_RCC_PLL1_Disable();
    LL_RCC_PLL2_Disable();
    LL_RCC_PLL3_Disable();

    // wait for ready
    while (LL_RCC_PLL1_IsReady() != 0U){}
    while (LL_RCC_PLL2_IsReady() != 0U){}
    while (LL_RCC_PLL3_IsReady() != 0U){}

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
}

/**
 * Configure phase locked loops
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_pll(void) {
    uint32_t clock_source;
    uint32_t pll_input_range;
    uint32_t pll_division;

    if (g_uni_hal_rcc_status.hse_inited) {
        clock_source = LL_RCC_PLLSOURCE_HSE;
        pll_input_range = LL_RCC_PLLINPUTRANGE_4_8;
        pll_division = 1;
    }
    else if(g_uni_hal_rcc_status.csi_inited){
        clock_source = LL_RCC_PLLSOURCE_CSI;
        pll_input_range = LL_RCC_PLLINPUTRANGE_4_8;
        pll_division = 1;
    }
    else if (g_uni_hal_rcc_status.hsi_inited) {
        clock_source = LL_RCC_PLLSOURCE_HSI;
        pll_input_range = LL_RCC_PLLINPUTRANGE_4_8;
        pll_division = 64'000'000 / HSE_VALUE;
    }
    else {
        clock_source = LL_RCC_PLLSOURCE_NONE;
    }

    if (clock_source != LL_RCC_PLLSOURCE_NONE) {
        _uni_hal_stm_rcc_pll_reset();

        // set source
        LL_RCC_PLL_SetSource(clock_source);

        // PLL1
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
        LL_RCC_PLL1_SetM(g_uni_hal_rcc_config->pll[0].m * pll_division);
        LL_RCC_PLL1_SetN(g_uni_hal_rcc_config->pll[0].n);
        LL_RCC_PLL1_Enable();

        // Wait till PLL is ready
        uint32_t ticks = uni_hal_systick_get_ms();
        while (!LL_RCC_PLL1_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.pll) {
        }

        g_uni_hal_rcc_status.pll_inited = LL_RCC_PLL1_IsReady();

        // PLL2
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
        if(start_pll_2) {
            LL_RCC_PLL2_SetVCOInputRange(pll_input_range);
            LL_RCC_PLL2_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);
            LL_RCC_PLL2_SetM(g_uni_hal_rcc_config->pll[1].m * pll_division);
            LL_RCC_PLL2_SetN(g_uni_hal_rcc_config->pll[1].n);
            LL_RCC_PLL2_Enable();

            // Wait till PLL2 is ready
            ticks = uni_hal_systick_get_ms();
            while (!LL_RCC_PLL2_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.pll) {
            }
        }

    }

    return g_uni_hal_rcc_status.pll_inited;
}

/**
 * Configure device system clock
 * @param ctx RCC context
 * @return true on success
 */
static void _uni_hal_stm_rcc_sysclk(void) {
    uint32_t clock_source;
    uint32_t clock_source_status;
    if (g_uni_hal_rcc_status.pll_inited) {
        clock_source = LL_RCC_SYS_CLKSOURCE_PLL1;
        clock_source_status = LL_RCC_SYS_CLKSOURCE_STATUS_PLL1;
    } else if (g_uni_hal_rcc_status.hse_inited) {
        clock_source = LL_RCC_SYS_CLKSOURCE_HSE;
        clock_source_status = LL_RCC_SYS_CLKSOURCE_STATUS_HSE;
    } else if (g_uni_hal_rcc_status.csi_inited) {
        clock_source = LL_RCC_SYS_CLKSOURCE_CSI;
        clock_source_status = LL_RCC_SYS_CLKSOURCE_STATUS_CSI;
    } else if (g_uni_hal_rcc_status.hsi_inited) {
        clock_source = LL_RCC_SYS_CLKSOURCE_HSI;
        clock_source_status = LL_RCC_SYS_CLKSOURCE_STATUS_HSI;
    }
    else{
        clock_source = 0U;
        clock_source_status = 0U;
    }

    // set prescalers
    LL_RCC_SetSysPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_2);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
    LL_RCC_SetAPB3Prescaler(LL_RCC_APB3_DIV_2);
    LL_RCC_SetAPB4Prescaler(LL_RCC_APB4_DIV_2);

    // Reconfigure sysclk
    LL_RCC_SetSysClkSource(clock_source);
    while (LL_RCC_GetSysClkSource() != clock_source_status) {
    }
    g_uni_hal_rcc_status.sys_inited = true;

    // Recalculate clock
    SystemCoreClockUpdate();
}



bool uni_hal_rcc_init(void) {
    if (g_uni_hal_rcc_config != nullptr && !g_uni_hal_rcc_status.inited) {
        // clear status
        _uni_hal_stm_rcc_reset();

        // enable configuration clocking
        uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_SYSCFG, true);
        uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_PWR, true);

        // set flash latency
        _uni_hal_stm_rcc_flash();

        // recalculate clock
        SystemCoreClockUpdate();

        // initialize systick
        uni_hal_systick_init();

        // HSE
        if(g_uni_hal_rcc_config->hse_enable) {
            _uni_hal_stm_rcc_hse();
        }

        // CSI
        if(g_uni_hal_rcc_config->csi_enable) {
            _uni_hal_stm_rcc_csi();
        }

        // HSI
        _uni_hal_stm_rcc_hsi();

        // LSE
        if(g_uni_hal_rcc_config->lse_enable != false) {
            _uni_hal_stm_rcc_lse();
        }

        // LSI
        if (!g_uni_hal_rcc_status.lse_inited) {
            _uni_hal_stm_rcc_lsi();
        }

        // PLL
        _uni_hal_stm_rcc_pll();

        // SYSCLK
        _uni_hal_stm_rcc_sysclk();

        // Systick once again
        uni_hal_systick_init();

        g_uni_hal_rcc_status.inited = true;
    }

    return g_uni_hal_rcc_status.inited;
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
        case UNI_HAL_CORE_PERIPH_PWR:
        case UNI_HAL_CORE_PERIPH_SYSCFG:
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
            case UNI_HAL_CORE_PERIPH_ETH_1:
                state ? LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_ETH1MAC) : LL_AHB1_GRP1_DisableClock(
                        LL_AHB1_GRP1_PERIPH_ETH1MAC);
                state ? LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_ETH1RX) : LL_AHB1_GRP1_DisableClock(
                        LL_AHB1_GRP1_PERIPH_ETH1RX);
                state ? LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_ETH1TX) : LL_AHB1_GRP1_DisableClock(
                        LL_AHB1_GRP1_PERIPH_ETH1TX);
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
        case UNI_HAL_CORE_PERIPH_SYSCLK:
            result = uni_hal_rcc_stm32h7_clk_get_hclk();
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

uni_hal_rcc_clksrc_e uni_hal_rcc_clksrc_get(uni_hal_core_periph_e target) {
    uni_hal_rcc_clksrc_e result = UNI_HAL_RCC_CLKSRC_UNKNOWN;
    if (uni_hal_rcc_is_inited()) {
        switch (target) {
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
            uni_hal_pwr_stm_set_backup_access(true);
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
            uni_hal_pwr_stm_set_backup_access(false);
            break;
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
            case UNI_HAL_CORE_PERIPH_I2C_3: {
                result = true;
                uint32_t src = 0;
                switch (source) {
                    case UNI_HAL_RCC_CLKSRC_PCLK1:
                        src = LL_RCC_I2C123_CLKSOURCE_PCLK1;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL3R:
                        src = LL_RCC_I2C123_CLKSOURCE_PLL3R;
                        break;
                    case UNI_HAL_RCC_CLKSRC_HSI:
                        src = LL_RCC_I2C123_CLKSOURCE_HSI;
                        break;
                    case UNI_HAL_RCC_CLKSRC_CSI:
                        src = LL_RCC_I2C123_CLKSOURCE_CSI;
                        break;
                    default:
                        result = false;
                }
                if (result) {
                    LL_RCC_SetI2CClockSource(src);
                }
                break;
            }
            case UNI_HAL_CORE_PERIPH_I2C_4: {
                result = true;
                uint32_t src = 0;
                switch (source) {
                    case UNI_HAL_RCC_CLKSRC_PCLK4:
                        src = LL_RCC_I2C4_CLKSOURCE_PCLK4;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL3R:
                        src = LL_RCC_I2C4_CLKSOURCE_PLL3R;
                        break;
                    case UNI_HAL_RCC_CLKSRC_HSI:
                        src = LL_RCC_I2C4_CLKSOURCE_HSI;
                        break;
                    case UNI_HAL_RCC_CLKSRC_CSI:
                        src = LL_RCC_I2C4_CLKSOURCE_CSI;
                        break;
                    default:
                        result = false;
                }
                if (result) {
                    LL_RCC_SetI2CClockSource(src);
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
            case UNI_HAL_CORE_PERIPH_RTC: {
                uint32_t src = LL_RCC_RTC_CLKSOURCE_NONE;
                if (source == UNI_HAL_RCC_CLKSRC_LSE) {
                    src = LL_RCC_RTC_CLKSOURCE_LSE;
                } else if (source == UNI_HAL_RCC_CLKSRC_LSI) {
                    src = LL_RCC_RTC_CLKSOURCE_LSI;
                } else if (source == UNI_HAL_RCC_CLKSRC_HSE) {
                    src = LL_RCC_RTC_CLKSOURCE_HSE;
                } else {
                    src = LL_RCC_RTC_CLKSOURCE_NONE;
                }

                uni_hal_pwr_stm_set_backup_access(true);
                LL_RCC_SetRTCClockSource(src);
                uni_hal_pwr_stm_set_backup_access(false);
                result = true;
                break;
            }
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
            case UNI_HAL_CORE_PERIPH_SPI_5:{
                uint32_t src = UINT32_MAX;
                switch (source) {
                    case UNI_HAL_RCC_CLKSRC_PCLK2:
                        src = LL_RCC_SPI45_CLKSOURCE_PCLK2;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL2Q:
                        src = LL_RCC_SPI45_CLKSOURCE_PLL2Q;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL3Q:
                        src = LL_RCC_SPI45_CLKSOURCE_PLL3Q;
                        break;
                    case UNI_HAL_RCC_CLKSRC_HSI:
                        src = LL_RCC_SPI45_CLKSOURCE_HSI;
                        break;
                    case UNI_HAL_RCC_CLKSRC_CSI:
                        src = LL_RCC_SPI45_CLKSOURCE_CSI;
                        break;
                    case UNI_HAL_RCC_CLKSRC_HSE:
                        src = LL_RCC_SPI45_CLKSOURCE_HSE;
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
            case UNI_HAL_CORE_PERIPH_UART_2:
            case UNI_HAL_CORE_PERIPH_UART_3:
            case UNI_HAL_CORE_PERIPH_UART_4:
            case UNI_HAL_CORE_PERIPH_UART_5:
            case UNI_HAL_CORE_PERIPH_UART_7:
            case UNI_HAL_CORE_PERIPH_UART_8: {
                uint32_t src = UINT32_MAX;
                switch (source) {
                    case UNI_HAL_RCC_CLKSRC_CSI:
                        src = LL_RCC_USART234578_CLKSOURCE_CSI;
                        break;
                    case UNI_HAL_RCC_CLKSRC_LSE:
                        src = LL_RCC_USART234578_CLKSOURCE_LSE;
                        break;
                    case UNI_HAL_RCC_CLKSRC_HSI:
                        src = LL_RCC_USART234578_CLKSOURCE_HSI;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PCLK1:
                        src = LL_RCC_USART234578_CLKSOURCE_PCLK1;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL2Q:
                        src = LL_RCC_USART234578_CLKSOURCE_PLL2Q;
                        break;
                    case UNI_HAL_RCC_CLKSRC_PLL3Q:
                        src = LL_RCC_USART234578_CLKSOURCE_PLL3Q;
                        break;
                    default:
                        // unknown clock source
                        break;
                }
                if (src != UINT32_MAX) {
                    LL_RCC_SetUSARTClockSource(src);
                    result = true;
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

bool uni_hal_rcc_stm32h7_config_set(uni_hal_rcc_stm32h7_config_t *config) {
    bool result = false;
    if (config != nullptr) {
        g_uni_hal_rcc_config = config;
        result = true;
    }
    return result;
}

uni_hal_rcc_stm32h7_status_t uni_hal_rcc_stm32h7_status_get() {
    return g_uni_hal_rcc_status;
}


uint32_t uni_hal_rcc_stm32h7_mco_enable(uint32_t mco_index, uni_hal_rcc_clksrc_e clock_source, uint32_t clock_divider) {
    bool result = false;
    if (mco_index < 2 && clock_divider >= 1 && clock_divider <= 15) {
        // configure pin
        uni_hal_gpio_pin_context_t pin = {
                .gpio_bank = mco_index == 0 ? UNI_HAL_CORE_PERIPH_GPIO_A : UNI_HAL_CORE_PERIPH_GPIO_C,
                .gpio_pin = mco_index == 0 ? UNI_HAL_GPIO_PIN_8 : UNI_HAL_GPIO_PIN_9,
                .gpio_speed = UNI_HAL_GPIO_SPEED_3,
                .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
                .alternate = UNI_HAL_GPIO_ALTERNATE_0,
        };
        result = uni_hal_gpio_pin_init(&pin);

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

        if (source != UINT32_MAX && prescaler != UINT32_MAX) {
            LL_RCC_ConfigMCO(source, prescaler);
            result = true;
        }
    }
    return result;
}



//
// STM32H7 - Interrupts
//


void NMI_Handler(void) {
    if (LL_RCC_IsActiveFlag_HSECSS()) {
        LL_RCC_ClearFlag_HSECSS();
        g_uni_hal_rcc_config->hse_enable = false;

        g_uni_hal_rcc_status.hse_inited = false;
        g_uni_hal_rcc_status.pll_inited = false;
        g_uni_hal_rcc_status.sys_inited = false;

        _uni_hal_stm_rcc_pll();
        _uni_hal_stm_rcc_sysclk();
    }
}



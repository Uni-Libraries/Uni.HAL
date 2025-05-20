//
// Includes
//

// ST
#include <stm32l4xx_ll_bus.h>
#include <stm32l4xx_ll_cortex.h>
#include <stm32l4xx_ll_rcc.h>
#include <stm32l4xx_ll_system.h>
#include <stm32l4xx_ll_utils.h>

// Uni.Common
#include <uni_common.h>

// UNI_HAL
#include "core/uni_hal_core.h"
#include "gpio/uni_hal_gpio.h"
#include "pwr/uni_hal_pwr_stm32.h"
#include "rcc/uni_hal_rcc.h"
#include "rcc/uni_hal_rcc_stm32l4.h"
#include "systick/uni_hal_systick.h"


//
// Globals
//

uni_hal_rcc_stm32l4_config_t *g_uni_hal_rcc_config = nullptr;
uni_hal_rcc_stm32l4_status_t g_uni_hal_rcc_status = {0};

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

static uint32_t _uni_hal_rcc_pll_get_m(uint32_t val)
{
    uint32_t result  = LL_RCC_PLLM_DIV_1;
    switch (val)
    {
    case 1:
        result = LL_RCC_PLLM_DIV_1;
        break;
    case 2:
        result = LL_RCC_PLLM_DIV_2;
        break;
    case 3:
        result = LL_RCC_PLLM_DIV_3;
        break;
    case 4:
        result = LL_RCC_PLLM_DIV_4;
        break;
    case 5:
        result = LL_RCC_PLLM_DIV_5;
        break;
    case 6:
        result = LL_RCC_PLLM_DIV_6;
        break;
    case 7:
        result = LL_RCC_PLLM_DIV_7;
        break;
    case 8:
        result = LL_RCC_PLLM_DIV_8;
        break;
    default:
        break;
    }
    return result;
}


static uint32_t _uni_hal_rcc_pll_get_n(uint32_t val)
{
    uint32_t result  = 8;
    if (val >= 8 && val <= 127)
    {
        result = val;
    }
    return result;
}


static uint32_t _uni_hal_rcc_pll_get_r(uint32_t val)
{
    uint32_t result  = LL_RCC_PLLR_DIV_2;
    switch (val)
    {
    case 2:
        result = LL_RCC_PLLR_DIV_2;
        break;
    case 4:
        result = LL_RCC_PLLR_DIV_4;
        break;
    case 6:
        result = LL_RCC_PLLR_DIV_6;
        break;
    case 8:
        result = LL_RCC_PLLR_DIV_8;
        break;
    default:
        break;
    }
    return result;
}


/**
 * Get HCLK frequency
 * @return HCLK frequency in hertz
 */
static uint32_t _uni_hal_rcc_get_hclk_freq(void) {
    // get HCLK freq
    LL_RCC_ClocksTypeDef clocks;
    LL_RCC_GetSystemClocksFreq(&clocks);
    return clocks.HCLK_Frequency;
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
static void _uni_hal_stm_rcc_flash(void) {
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_4) {
    }
}


/**
 * Configure high speed external clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_hse() {
    bool result = false;

    if (g_uni_hal_rcc_config->hse_bypass)
    {
        LL_RCC_HSE_EnableBypass();
    }

    // Enable HSE
    LL_RCC_HSE_Enable();

    // Wait till HSE is ready
    uint32_t ticks = uni_hal_systick_get_ms();
    while (!LL_RCC_HSE_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.hse) {
    }

    // save HSE status
    result = g_uni_hal_rcc_status.hse_inited = LL_RCC_HSE_IsReady();

    // enable HSE
    if (result && g_uni_hal_rcc_config->hse_css) {
        LL_RCC_HSE_EnableCSS();
    }

    return result;
}

/**
 * Configure high speed internal clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_hsi() {
    bool result = false;

    LL_RCC_HSI_Enable();

    /* Wait till HSI is ready */
    uint32_t ticks = uni_hal_systick_get_ms();
    while (!LL_RCC_HSI_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.hsi) {
    }

    result = g_uni_hal_rcc_status.hsi_inited = LL_RCC_HSI_IsReady();

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
    // setup SysTick
    NVIC_SetPriority(SysTick_IRQn, 0U);
    LL_InitTick(_uni_hal_rcc_get_hclk_freq(), 1000U);
    LL_SYSTICK_EnableIT();

    return true;
}

/**
 * Configure low speed external clockAHBPrescTable
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_lse() {
    bool result = false;

    // Check LSE_IsReady flag
    result = g_uni_hal_rcc_status.lse_inited = LL_RCC_LSE_IsReady();

    if (!result) {
        uni_hal_pwr_stm_set_backup_access(true);
        LL_RCC_LSE_SetDriveCapability(LL_RCC_LSEDRIVE_MEDIUMLOW);
        LL_RCC_LSE_Enable();

        /* Wait till LSE is ready */
        uint32_t ticks = uni_hal_systick_get_ms();
        while (!LL_RCC_LSE_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.lse) {
        }
        uni_hal_pwr_stm_set_backup_access(false);

        // Check LSE_IsReady flag
        result = g_uni_hal_rcc_status.lse_inited = LL_RCC_LSE_IsReady();

        // Try one time to reset Backup Domain and init LSE again
        if (!result && !g_uni_hal_rcc_status.lse_backup_reseted) {
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
    uint32_t ticks = uni_hal_systick_get_ms();
    while (!LL_RCC_LSI_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.lsi) {
    }

    result = g_uni_hal_rcc_status.lsi_inited = LL_RCC_LSI_IsReady();


    return result;
}

/**
 * Configure phase locked loops
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_pll() {
    bool result = false;

    uint32_t clock_source = LL_RCC_PLLSOURCE_NONE;
    uint32_t pll_division = 1;

    if (g_uni_hal_rcc_status.hse_inited) {
        clock_source = LL_RCC_PLLSOURCE_HSE;
        pll_division = 1;
    } else if (g_uni_hal_rcc_status.hsi_inited) {
        clock_source = LL_RCC_PLLSOURCE_HSI;
        pll_division = HSI_VALUE / HSE_VALUE;
    }

    if (clock_source != LL_RCC_PLLSOURCE_NONE) {
        //PPL1
        LL_RCC_PLL_ConfigDomain_SYS(clock_source,
            _uni_hal_rcc_pll_get_m(g_uni_hal_rcc_config->pll[0].m * pll_division),
            _uni_hal_rcc_pll_get_n(g_uni_hal_rcc_config->pll[0].n),
            _uni_hal_rcc_pll_get_r(g_uni_hal_rcc_config->pll[0].r));
        LL_RCC_PLL_EnableDomain_SYS();

        LL_RCC_PLL_Enable();

        /* Wait till PLL is ready */
        uint32_t ticks = uni_hal_systick_get_ms();
        while (!LL_RCC_PLL_IsReady() && (uni_hal_systick_get_ms() - ticks) < g_uni_hal_rcc_config->timeout.pll) {
        }

        result = g_uni_hal_rcc_status.pll_inited = LL_RCC_PLL_IsReady();
    }

    return result;
}

/**
 * Configure device system clock
 * @param ctx RCC context
 * @return true on success
 */
static bool _uni_hal_stm_rcc_sysclk() {
    bool result = false;

    uint32_t clock_source = 0;
    uint32_t clock_source_status = 0;

    if (g_uni_hal_rcc_status.pll_inited) {
        clock_source = LL_RCC_SYS_CLKSOURCE_PLL;
        clock_source_status = LL_RCC_SYS_CLKSOURCE_STATUS_PLL;
    } else if (g_uni_hal_rcc_status.hse_inited) {
        clock_source = LL_RCC_SYS_CLKSOURCE_HSE;
        clock_source_status = LL_RCC_SYS_CLKSOURCE_STATUS_HSE;
    } else if (g_uni_hal_rcc_status.hsi_inited) {
        clock_source = LL_RCC_SYS_CLKSOURCE_HSI;
        clock_source_status = LL_RCC_SYS_CLKSOURCE_STATUS_HSI;
    }

    // Reconfigure sysclk
    LL_RCC_SetSysClkSource(clock_source);

    /* Wait till System clock is ready */
    uint32_t ticks = uni_hal_systick_get_ms();
    while (LL_RCC_GetSysClkSource() != clock_source_status && (uni_hal_systick_get_ms() - ticks) <
            g_uni_hal_rcc_config->timeout.pll) {
    }

    result = LL_RCC_GetSysClkSource() == clock_source_status;
    g_uni_hal_rcc_status.sys_inited = result;

    if (result) {
        // Set prescalers
        LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
        LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
        LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);

        // Update sysclk value
        SystemCoreClockUpdate();

        // Reconfigure systick
        _uni_hal_rcc_systick();
    }

    return result;
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

    if (g_uni_hal_rcc_status.pll_inited) {
        LL_RCC_SetRNGClockSource(LL_RCC_RNG_CLKSOURCE_PLL);
    } else {
        LL_RCC_SetRNGClockSource(LL_RCC_RNG_CLKSOURCE_HSI48);
    }

    LL_RCC_SetUARTClockSource(LL_RCC_UART4_CLKSOURCE_PCLK1);

    LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);
    LL_RCC_SetUSARTClockSource(LL_RCC_USART2_CLKSOURCE_PCLK1);
}

static bool _uni_hal_rcc_update_status_reg() {
    bool result = false;
    //TODO g_uni_hal_rcc_status.rcc_csr_reg = (uint8_t) ((RCC->CSR) >> 24);
    LL_RCC_ClearResetFlags();
    result = true;

    return result;
}


bool uni_hal_rcc_init() {
    bool result = false;

    // enable configuration clocking
    uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_SYSCFG, true);
    uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_PWR, true);

    // set flash latency
    _uni_hal_stm_rcc_flash();

    // initialize systick
    _uni_hal_rcc_systick();

    // basic settings
    _uni_hal_rcc_update_status_reg();
    // TODO: move to another file

    // HSE
    if (g_uni_hal_rcc_config->hse_enable)
    {
        _uni_hal_stm_rcc_hse();
    }

    // HSI
    _uni_hal_stm_rcc_hsi();

    // LSE/LSI
    _uni_hal_stm_rcc_lse();
    g_uni_hal_rcc_status.lse_inited = false; //TODO: remove
    if (!g_uni_hal_rcc_status.lse_inited) {
        _uni_hal_stm_rcc_lsi();
    }

    // PLL
    _uni_hal_stm_rcc_pll();

    // SYSCLK
    _uni_hal_stm_rcc_sysclk();

    // PERIPHERIALS
    _uni_hal_stm_rcc_peripherials();

    result = true;
    g_uni_hal_rcc_status.inited = true;

    return result;
}


bool uni_hal_rcc_is_inited() {
    return g_uni_hal_rcc_status.inited;
}

uint8_t uni_hal_rcc_get_status_reg() {
    uint8_t result = 0;
    if (uni_hal_rcc_is_inited()) {
        result = 0; //TODO: g_uni_hal_rcc_status.rcc_csr_reg;
    }
    return result;
}


bool uni_hal_rcc_get_status_reg_flag(uint8_t flag) {
    bool result = false;
    if (uni_hal_rcc_is_inited()) {
        result = flag != 0; //TODO g_uni_hal_rcc_status.rcc_csr_reg & flag;
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

    if (state != uni_hal_rcc_clk_get(target)) {
        result = false;
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
            case UNI_HAL_CORE_PERIPH_RTC:
                uni_hal_pwr_stm_set_backup_access(true);
            state ? LL_RCC_EnableRTC() : LL_RCC_DisableRTC();
            uni_hal_pwr_stm_set_backup_access(false);
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
                    : LL_APB2_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_USART2);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_UART_3:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3)
                    : LL_APB2_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_USART3);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_UART_4:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART4)
                    : LL_APB2_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_UART4);
            result = true;
            break;
            case UNI_HAL_CORE_PERIPH_UART_5:
                state
                    ? LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART5)
                    : LL_APB2_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_UART5);
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
            case UNI_HAL_CORE_PERIPH_SYSCLK:
                result = _uni_hal_rcc_get_hclk_freq();
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
                result = _uni_hal_rcc_get_pclk2_freq();
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
                    case LL_RCC_RTC_CLKSOURCE_HSE_DIV32:
                        result = UNI_HAL_RCC_CLKSRC_HSE_DIV_32;
                        break;
                    default:
                        break;
                }
                uni_hal_pwr_stm_set_backup_access(false);
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
            case UNI_HAL_CORE_PERIPH_RTC: {
                uint32_t src = LL_RCC_RTC_CLKSOURCE_NONE;
                if (source == UNI_HAL_RCC_CLKSRC_LSE) {
                    src = LL_RCC_RTC_CLKSOURCE_LSE;
                } else if (source == UNI_HAL_RCC_CLKSRC_LSI) {
                    src = LL_RCC_RTC_CLKSOURCE_LSI;
                } else if (source == UNI_HAL_RCC_CLKSRC_HSE_DIV_32) {
                    src = LL_RCC_RTC_CLKSOURCE_HSE_DIV32;
                } else {
                    src = LL_RCC_RTC_CLKSOURCE_NONE;
                }

                uni_hal_pwr_stm_set_backup_access(true);
                LL_RCC_SetRTCClockSource(src);
                uni_hal_pwr_stm_set_backup_access(false);
                result = true;
                break;
            }
            default:
                break;
        }
    }

    return result;
}

bool uni_hal_rcc_stm32l4_config_set(uni_hal_rcc_stm32l4_config_t* config)
{
    bool result = false;
    if (config != nullptr) {
        g_uni_hal_rcc_config = config;
        result = true;
    }
    return result;
}


uint32_t uni_hal_rcc_stm32_mco_enable(uint32_t mco_index, uni_hal_rcc_clksrc_e clock_source, uint32_t clock_divider) {
    bool result = false;
    if (mco_index == 0 && (clock_divider == 1 || clock_divider == 2 || clock_divider == 4 || clock_divider == 8 || clock_divider == 16)) {
        // configure pin
        uni_hal_gpio_pin_context_t pin = {
                .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_A,
                .gpio_pin = UNI_HAL_GPIO_PIN_8,
                .gpio_speed = UNI_HAL_GPIO_SPEED_3,
                .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
                .alternate = UNI_HAL_GPIO_ALTERNATE_0,
        };
        result = uni_hal_gpio_pin_init(&pin);

        uint32_t source = UINT32_MAX;
        uint32_t prescaler = UINT32_MAX;
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
                break;
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
// STM32L4 - Interrupts
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

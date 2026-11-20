// Run the L496 driver and ST's LL register accesses against peripheral registers in RAM.
#include <stdbool.h>
#include <stddef.h>

#define STM32L496xx
#define USE_FULL_LL_DRIVER
#define HSE_VALUE 12'000'000U
#include <stm32l4xx.h>

static RCC_TypeDef rcc;
static RNG_TypeDef rng;
static PWR_TypeDef pwr;
static FLASH_TypeDef flash;
rcc_test_systick_t rcc_test_systick;
#undef RCC
#undef RNG
#undef PWR
#undef FLASH
#define RCC (&rcc)
#define RNG (&rng)
#define PWR (&pwr)
#define FLASH (&flash)

#include <stm32l4xx_ll_pwr.h>
#include <stm32l4xx_ll_rcc.h>
#include <stm32l4xx_ll_rng.h>
#include <stm32l4xx_ll_system.h>
#include "../../3rdparty/st_stm32l4/src_hal/stm32l4xx_ll_rcc.c"

bool rcc_test_pll_locks;
bool rcc_test_hsi48_locks;
bool rcc_test_can_switch;
bool rcc_test_late_pll;
uint32_t rcc_test_msi_delay;
bool rcc_test_fault_on_switch;
void rcc_test_fail_hse(void);
bool rcc_test_flash_accepts;
bool rcc_test_flash_can_lower;
uint32_t rcc_test_tick_reads;
uint32_t rcc_test_tick_inits;

static uint32_t pll_ready(void) {
    if (rcc_test_pll_locks && (rcc.CR & RCC_CR_PLLON)) {
        rcc.CR |= RCC_CR_PLLRDY;
    } else {
        rcc.CR &= ~RCC_CR_PLLRDY;
    }
    return LL_RCC_PLL_IsReady();
}

void rcc_test_complete_switch(void) {
    uint32_t pending = rcc.CFGR & RCC_CFGR_SW;
    MODIFY_REG(rcc.CFGR, RCC_CFGR_SWS, pending << RCC_CFGR_SWS_Pos);
}

uint32_t rcc_test_hclk(void) {
    LL_RCC_ClocksTypeDef clocks;
    LL_RCC_GetSystemClocksFreq(&clocks);
    return clocks.HCLK_Frequency;
}

static uint32_t clock_status(void) {
    if (rcc_test_msi_delay && (rcc.CFGR & RCC_CFGR_SW) == LL_RCC_SYS_CLKSOURCE_MSI &&
        LL_RCC_GetSysClkSource() == LL_RCC_SYS_CLKSOURCE_STATUS_PLL && --rcc_test_msi_delay == 0U) {
        rcc_test_complete_switch();
    }
    return LL_RCC_GetSysClkSource();
}

static void switch_clock(uint32_t source) {
    if (rcc_test_late_pll && (rcc.CFGR & RCC_CFGR_SW) == LL_RCC_SYS_CLKSOURCE_PLL &&
        source == LL_RCC_SYS_CLKSOURCE_MSI) {
        // The previous request takes effect just before the cancellation write.
        rcc_test_complete_switch();
        rcc_test_late_pll = false;
    }
    LL_RCC_SetSysClkSource(source);
    if (rcc_test_fault_on_switch) {
        rcc_test_fault_on_switch = false;
        rcc_test_fail_hse();
        return;
    }
    if (rcc_test_can_switch) {
        rcc_test_complete_switch();
    }
}

static void write_flash_latency(uint32_t latency) {
    if (rcc_test_flash_accepts && (latency == LL_FLASH_LATENCY_4 || rcc_test_flash_can_lower)) {
        LL_FLASH_SetLatency(latency);
    }
}

static void start_hsi48(void) {
    LL_RCC_HSI48_Enable();
    if (rcc_test_hsi48_locks) {
        rcc.CRRCR |= RCC_CRRCR_HSI48RDY;
    }
}

#define LL_RCC_HSI48_Enable start_hsi48
#define LL_RCC_PLL_IsReady pll_ready
#define LL_RCC_SetSysClkSource switch_clock
#define LL_RCC_GetSysClkSource clock_status
#define LL_FLASH_SetLatency write_flash_latency
// This vendor helper uses a literal 32-bit MMIO address. I2C mux selection is outside this fixture.
#define LL_RCC_SetI2CClockSource(source) ((void)(source))
#include "systick/uni_hal_systick_reclock_stm32.c"
#include "rcc/uni_hal_rcc_stm32l4.c"

static uint32_t rng_word_ready(RNG_TypeDef *instance) {
    uint32_t frequency = LL_RCC_GetRNGClockFreq(LL_RCC_RNG_CLKSOURCE);
    return LL_RNG_IsEnabled(instance) && uni_hal_rcc_clk_get(UNI_HAL_CORE_PERIPH_RNG) &&
        frequency != 0U && frequency >= SystemCoreClock / 16U;
}

static uint32_t rng_test_ms(void) {
    static uint32_t tick;
    return tick++;
}

#define LL_RNG_IsActiveFlag_DRDY rng_word_ready
#define uni_hal_systick_get_ms rng_test_ms
#include "rng/uni_hal_rng_stm32l4.c"
#undef uni_hal_systick_get_ms

bool rcc_test_hsi48_enabled(void) {
    return (rcc.CRRCR & RCC_CRRCR_HSI48ON) != 0U;
}

void rcc_test_select_msi24(void) {
    LL_RCC_MSI_SetRange(LL_RCC_MSIRANGE_9);
    LL_RCC_SetRNGClockSource(LL_RCC_RNG_CLKSOURCE_MSI);
}

bool rcc_test_msi_selected(void) {
    return LL_RCC_GetRNGClockSource(LL_RCC_RNG_CLKSOURCE) == LL_RCC_RNG_CLKSOURCE_MSI;
}

bool uni_hal_systick_init(void) {
    ++rcc_test_tick_inits;
    SystemCoreClockUpdate();
    SysTick->LOAD = SystemCoreClock / 1000U - 1U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | 3U;
    return true;
}

uint32_t uni_hal_systick_get_freq(void) {
    return 1000;
}

uint32_t uni_hal_systick_get_ms(void) {
    // A frozen tick is intentional, including when timeout.pll is non-zero.
    ++rcc_test_tick_reads;
    return 0;
}

void SystemCoreClockUpdate(void) {
    LL_RCC_ClocksTypeDef clocks;
    LL_RCC_GetSystemClocksFreq(&clocks);
    SystemCoreClock = clocks.HCLK_Frequency;
}

void uni_hal_pwr_stm_set_backup_access(bool enable) {
    (void)enable;
}

void rcc_test_reset(void) {
    rcc = (RCC_TypeDef){0};
    rng = (RNG_TypeDef){.DR = 0x6429DC01U};
    rcc_test_hsi48_locks = true;
    pwr.CR1 = LL_PWR_REGU_VOLTAGE_SCALE1;
    flash.ACR = 0;
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    rcc.CR = RCC_CR_MSIRDY | RCC_CR_MSIRGSEL | LL_RCC_MSIRANGE_6 |
        RCC_CR_HSERDY | RCC_CR_HSIRDY;
    rcc.CSR = RCC_CSR_LSIRDY;
    g_uni_hal_rcc_config = NULL;
    g_uni_hal_rcc_status = (uni_hal_rcc_stm32l4_status_t){0};
    rcc_test_fault_on_switch = false;
    rcc_test_pll_locks = true;
    rcc_test_can_switch = true;
    rcc_test_late_pll = false;
    rcc_test_msi_delay = 0U;
    rcc_test_flash_accepts = true;
    rcc_test_flash_can_lower = true;
    rcc_test_tick_reads = 0;
    rcc_test_tick_inits = 0;
}

void rcc_test_fail_hse(void) {
    // CSS switched to MSI (STOPWUCK = 0), disabled HSE and the HSE-fed system PLL.
    rcc.CR &= ~(RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY);
    MODIFY_REG(rcc.CFGR, RCC_CFGR_SW | RCC_CFGR_SWS, LL_RCC_SYS_CLKSOURCE_MSI);
    rcc.CIFR = RCC_CIFR_CSSF;
    NMI_Handler();
}

uint32_t rcc_test_m(void) {
    return 1 + (LL_RCC_PLL_GetDivider() >> RCC_PLLCFGR_PLLM_Pos);
}

uint32_t rcc_test_n(void) {
    return LL_RCC_PLL_GetN();
}

uint32_t rcc_test_p(void) {
    if ((rcc.PLLCFGR & RCC_PLLCFGR_PLLPEN) == 0) {
        return 0;
    }
    uint32_t divider = (rcc.PLLCFGR & RCC_PLLCFGR_PLLPDIV) >> RCC_PLLCFGR_PLLPDIV_Pos;
    return divider ? divider : (rcc.PLLCFGR & RCC_PLLCFGR_PLLP ? 17U : 7U);
}

uint32_t rcc_test_q(void) {
    return (rcc.PLLCFGR & RCC_PLLCFGR_PLLQEN) ?
        2U * (1U + ((rcc.PLLCFGR & RCC_PLLCFGR_PLLQ) >> RCC_PLLCFGR_PLLQ_Pos)) : 0U;
}

void rcc_test_use_fixed_p(bool div17) {
    MODIFY_REG(rcc.PLLCFGR, RCC_PLLCFGR_PLLPDIV | RCC_PLLCFGR_PLLP, div17 ? RCC_PLLCFGR_PLLP : 0U);
}

bool rcc_test_css_acknowledged(void) {
    return (rcc.CICR & RCC_CICR_CSSC) != 0;
}

bool rcc_test_requested_pll(void) {
    return (rcc.CFGR & RCC_CFGR_SW) == LL_RCC_SYS_CLKSOURCE_PLL;
}

void rcc_test_hsi_not_ready(void) {
    rcc.CR &= ~RCC_CR_HSIRDY;
}

void rcc_test_voltage_range2(void) {
    pwr.CR1 = LL_PWR_REGU_VOLTAGE_SCALE2;
}

void rcc_test_divide_ahb_by_four(void) {
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_4);
    SystemCoreClockUpdate();
}

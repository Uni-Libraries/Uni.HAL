//
// Includes
//

// st
#include <stm32h7xx_hal.h>
#include <stm32h7xx_hal_flash.h>
#include <stm32h7xx_hal_flash_ex.h>
#include <stm32h7xx_hal_rcc.h>
#include <stm32h7xx_ll_rcc.h>
#include <stm32h7xx_ll_system.h>
#include <stm32h7xx_ll_pwr.h>

// FreeRTOS
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

// Uni.HAL
#include "core/uni_hal_core.h"
#include "dwt/uni_hal_dwt.h"
#include "flash/uni_hal_flash.h"
#include "os/uni_hal_os.h"
#include "systick/uni_hal_systick.h"


//
// Globals
//

static volatile SemaphoreHandle_t xFlashDoneSem;
static volatile bool flash_initialized;
static volatile bool flash_owned;
static volatile bool relock_control, relock_options;
typedef enum { ERASE_IDLE, ERASE_WAITING, ERASE_OK, ERASE_FAILED, ERASE_TIMED_OUT } flash_erase_state_t;
static struct {
    volatile flash_erase_state_t state;
    bool notify_task;
    uni_hal_flash_bank_e bank;
    uintptr_t address;
} sector_erase;
static flash_erase_state_t irq_result;


enum {
    UNI_HAL_FLASH_LATENCY_TIMEOUT_MS = 100U,
    // same budget as FLASH_TIMEOUT_VALUE of the ST HAL blocking erase
    UNI_HAL_FLASH_ERASE_TIMEOUT_MS = 50000U,
};


typedef struct {
    bool dwt_ready;
    uint32_t start_tick;
    uint32_t timeout_tick;
    uint32_t start_ms;
    uint32_t timeout_ms;
} uni_hal_flash_wait_timeout_t;


typedef struct {
    uint32_t max_hz;
    uint32_t latency;
    uint32_t wrhighfreq;
} uni_hal_flash_latency_step_t;

//
// HAL
//

// ST calls these before clearing flags, disabling interrupts and releasing pFlash.
void HAL_FLASH_EndOfOperationCallback(uint32_t value) {
    if (value == UINT32_MAX && irq_result != ERASE_FAILED) { // final sector of an interrupt-driven sector erase
        irq_result = ERASE_OK;
    }
}

void HAL_FLASH_OperationErrorCallback(uint32_t value) {
    (void)value;
    irq_result = ERASE_FAILED;
}

// Polling operations may time out with either a bank or the option bytes still busy.
static bool controller_idle(void) {
    return ((FLASH->SR1 | FLASH->SR2) & (FLASH_SR_QW | FLASH_SR_BSY | FLASH_SR_WBNE)) == 0U &&
           (FLASH->OPTSR_CUR & FLASH_OPTSR_OPT_BUSY) == 0U;
}

static bool lock_flash(void) {
    relock_control = !controller_idle() || HAL_FLASH_Lock() != HAL_OK;
    return !relock_control;
}

static bool lock_options(void) {
    relock_options = !controller_idle() || HAL_FLASH_OB_Lock() != HAL_OK;
    return !relock_options;
}

static bool finish_erase(void) {
    volatile uint32_t *cr = sector_erase.bank == UNI_HAL_FLASH_BANK_1 ? &FLASH->CR1 : &FLASH->CR2;
    CLEAR_BIT(*cr, FLASH_CR_SER | FLASH_CR_SNB);
    bool const locked = lock_flash();
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U) {
        SCB_InvalidateDCache_by_Addr((void *)sector_erase.address, FLASH_SECTOR_SIZE);
    }
    if ((SCB->CCR & SCB_CCR_IC_Msk) != 0U) {
        SCB_InvalidateICache();
    }
    return locked;
}

void FLASH_IRQHandler(void) {
    UNI_HAL_OS_ISR_ENTER();
    BaseType_t woken = pdFALSE;
    irq_result = ERASE_IDLE;
    HAL_FLASH_IRQHandler();
    if (irq_result != ERASE_IDLE) {
        if (sector_erase.state == ERASE_WAITING) {
            sector_erase.state = irq_result;
            if (sector_erase.notify_task) {
                xSemaphoreGiveFromISR(xFlashDoneSem, &woken);
            }
        } else if (sector_erase.state == ERASE_TIMED_OUT) {
            // The caller has returned. Only the terminal IRQ may retire this operation.
            (void)finish_erase();
            sector_erase.state = ERASE_IDLE;
        }
    }
    UNI_HAL_OS_ISR_EXIT(woken);
}


//
// Private
//

static void _uni_hal_flash_wait_timeout_begin(uni_hal_flash_wait_timeout_t* timeout_ctx, uint32_t timeout_ms) {
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


static bool _uni_hal_flash_wait_timeout_expired(const uni_hal_flash_wait_timeout_t* timeout_ctx) {
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


static uint32_t _uni_hal_flash_vos_to_index(uint32_t vos_value) {
    if (vos_value == LL_PWR_REGU_VOLTAGE_SCALE0) {
        return 0U;
    }
    if (vos_value == LL_PWR_REGU_VOLTAGE_SCALE1) {
        return 1U;
    }
    if (vos_value == LL_PWR_REGU_VOLTAGE_SCALE2) {
        return 2U;
    }
    if (vos_value == LL_PWR_REGU_VOLTAGE_SCALE3) {
        return 3U;
    }

    return UINT32_MAX;
}


static uint32_t _uni_hal_flash_calc_pll_source_hz(void) {
    switch (LL_RCC_PLL_GetSource()) {
        case LL_RCC_PLLSOURCE_HSE:
            return HSE_VALUE;

        case LL_RCC_PLLSOURCE_CSI:
            return CSI_VALUE;

        case LL_RCC_PLLSOURCE_HSI: {
            uint32_t const hsidiv = __HAL_RCC_GET_HSI_DIVIDER();
            if (hsidiv == RCC_CR_HSIDIV_2) {
                return HSI_VALUE / 2U;
            }
            if (hsidiv == RCC_CR_HSIDIV_4) {
                return HSI_VALUE / 4U;
            }
            if (hsidiv == RCC_CR_HSIDIV_8) {
                return HSI_VALUE / 8U;
            }
            return HSI_VALUE;
        }

        default:
            return 0U;
    }
}


static uint32_t _uni_hal_flash_calc_pll1_p_hz(void) {
    if (LL_RCC_PLL1P_IsEnabled() == 0U) {
        return 0U;
    }

    uint32_t const pll_input_hz = _uni_hal_flash_calc_pll_source_hz();
    if (pll_input_hz == 0U) {
        return 0U;
    }

    uint32_t const pll_m = LL_RCC_PLL1_GetM();
    uint32_t const pll_p = LL_RCC_PLL1_GetP();
    uint32_t const pll_n = LL_RCC_PLL1_GetN();
    if ((pll_m == 0U) || (pll_p == 0U) || (pll_n == 0U)) {
        return 0U;
    }

    uint32_t const pll_fracn = LL_RCC_PLL1_GetFRACN();
    uint64_t const vco_input_hz = ((uint64_t)pll_input_hz) / pll_m;
    uint64_t const vco_mult_q13 = ((uint64_t)pll_n << 13U) + pll_fracn;
    uint64_t const vco_hz = (vco_input_hz * vco_mult_q13) >> 13U;
    uint64_t const pll_p_hz = vco_hz / pll_p;

    return pll_p_hz > (uint64_t)UINT32_MAX ? UINT32_MAX : (uint32_t)pll_p_hz;
}


static uint32_t _uni_hal_flash_calc_sysclk_hz(void) {
    uint32_t const sysclk_src = LL_RCC_GetSysClkSource();

    if (sysclk_src == LL_RCC_SYS_CLKSOURCE_STATUS_PLL1) {
        return _uni_hal_flash_calc_pll1_p_hz();
    }

    if (sysclk_src == LL_RCC_SYS_CLKSOURCE_STATUS_HSE) {
        return HSE_VALUE;
    }

    if (sysclk_src == LL_RCC_SYS_CLKSOURCE_STATUS_CSI) {
        return CSI_VALUE;
    }

    if (sysclk_src == LL_RCC_SYS_CLKSOURCE_STATUS_HSI) {
        uint32_t const hsidiv = __HAL_RCC_GET_HSI_DIVIDER();
        if (hsidiv == RCC_CR_HSIDIV_2) {
            return HSI_VALUE / 2U;
        }
        if (hsidiv == RCC_CR_HSIDIV_4) {
            return HSI_VALUE / 4U;
        }
        if (hsidiv == RCC_CR_HSIDIV_8) {
            return HSI_VALUE / 8U;
        }
        return HSI_VALUE;
    }

    return 0U;
}


static uint32_t _uni_hal_flash_calc_axi_clock_hz(void) {
    uint32_t const sysclk_hz = _uni_hal_flash_calc_sysclk_hz();
    if (sysclk_hz == 0U) {
        return 0U;
    }

    return LL_RCC_CALC_HCLK_FREQ(sysclk_hz, LL_RCC_GetAHBPrescaler());
}


static uint32_t _uni_hal_flash_select_latency(uint32_t vos_index, uint32_t axi_clock_hz, uint32_t* wrhighfreq_out) {
    if (wrhighfreq_out == nullptr) {
        return UINT32_MAX;
    }

    // STM32H7 datasheet: FLASH recommended wait states / WRHIGHFREQ table.
    static const uni_hal_flash_latency_step_t k_limits_vos0[] = {
        { 70'000'000U, LL_FLASH_LATENCY_0, 0U },
        { 140'000'000U, LL_FLASH_LATENCY_1, FLASH_ACR_WRHIGHFREQ_0 },
        { 185'000'000U, LL_FLASH_LATENCY_2, FLASH_ACR_WRHIGHFREQ_0 },
        { 210'000'000U, LL_FLASH_LATENCY_2, FLASH_ACR_WRHIGHFREQ_1 },
        { 225'000'000U, LL_FLASH_LATENCY_3, FLASH_ACR_WRHIGHFREQ_1 },
        { 240'000'000U, LL_FLASH_LATENCY_4, FLASH_ACR_WRHIGHFREQ_1 },
    };

    static const uni_hal_flash_latency_step_t k_limits_vos1[] = {
        { 70'000'000U, LL_FLASH_LATENCY_0, 0U },
        { 140'000'000U, LL_FLASH_LATENCY_1, FLASH_ACR_WRHIGHFREQ_0 },
        { 185'000'000U, LL_FLASH_LATENCY_2, FLASH_ACR_WRHIGHFREQ_0 },
        { 210'000'000U, LL_FLASH_LATENCY_2, FLASH_ACR_WRHIGHFREQ_1 },
        { 225'000'000U, LL_FLASH_LATENCY_3, FLASH_ACR_WRHIGHFREQ_1 },
    };

    static const uni_hal_flash_latency_step_t k_limits_vos2[] = {
        { 55'000'000U, LL_FLASH_LATENCY_0, 0U },
        { 110'000'000U, LL_FLASH_LATENCY_1, FLASH_ACR_WRHIGHFREQ_0 },
        { 165'000'000U, LL_FLASH_LATENCY_2, FLASH_ACR_WRHIGHFREQ_0 },
        { 225'000'000U, LL_FLASH_LATENCY_3, FLASH_ACR_WRHIGHFREQ_1 },
    };

    static const uni_hal_flash_latency_step_t k_limits_vos3[] = {
        { 45'000'000U, LL_FLASH_LATENCY_0, 0U },
        { 90'000'000U, LL_FLASH_LATENCY_1, FLASH_ACR_WRHIGHFREQ_0 },
        { 135'000'000U, LL_FLASH_LATENCY_2, FLASH_ACR_WRHIGHFREQ_0 },
        { 180'000'000U, LL_FLASH_LATENCY_3, FLASH_ACR_WRHIGHFREQ_1 },
        { 225'000'000U, LL_FLASH_LATENCY_4, FLASH_ACR_WRHIGHFREQ_1 },
    };

    uni_hal_flash_latency_step_t const* limits = nullptr;
    size_t limit_count = 0U;

    switch (vos_index) {
        case 0U:
            limits = k_limits_vos0;
            limit_count = sizeof(k_limits_vos0) / sizeof(k_limits_vos0[0]);
            break;
        case 1U:
            limits = k_limits_vos1;
            limit_count = sizeof(k_limits_vos1) / sizeof(k_limits_vos1[0]);
            break;
        case 2U:
            limits = k_limits_vos2;
            limit_count = sizeof(k_limits_vos2) / sizeof(k_limits_vos2[0]);
            break;
        case 3U:
            limits = k_limits_vos3;
            limit_count = sizeof(k_limits_vos3) / sizeof(k_limits_vos3[0]);
            break;
        default:
            return UINT32_MAX;
    }

    for (size_t i = 0U; i < limit_count; i++) {
        if (axi_clock_hz <= limits[i].max_hz) {
            *wrhighfreq_out = limits[i].wrhighfreq;
            return limits[i].latency;
        }
    }

    return UINT32_MAX;
}


static bool _uni_hal_flash_apply_latency(uint32_t latency, uint32_t wrhighfreq) {
    MODIFY_REG(FLASH->ACR, FLASH_ACR_WRHIGHFREQ, wrhighfreq);
    LL_FLASH_SetLatency(latency);

    uni_hal_flash_wait_timeout_t timeout_ctx = { 0 };
    _uni_hal_flash_wait_timeout_begin(&timeout_ctx, UNI_HAL_FLASH_LATENCY_TIMEOUT_MS);

    while (LL_FLASH_GetLatency() != latency) {
        if (_uni_hal_flash_wait_timeout_expired(&timeout_ctx)) {
            return false;
        }
    }

    return true;
}


//
// Public
//

static bool prepare_flash(bool need_event) {
    if (flash_initialized && (!need_event || xFlashDoneSem != nullptr)) {
        return true;
    }
    SemaphoreHandle_t candidate = nullptr;
    if (need_event && xFlashDoneSem == nullptr) {
        candidate = xSemaphoreCreateBinary();
        if (candidate == nullptr) {
            return false;
        }
    }

    // Creation can yield. Publish one event and configure its vector only once.
    uint32_t const mask = uni_hal_core_irq_pause();
    if (xFlashDoneSem == nullptr && candidate != nullptr) {
        xFlashDoneSem = candidate;
        candidate = nullptr;
    }
    if (!flash_initialized) {
        HAL_NVIC_SetPriority(FLASH_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(FLASH_IRQn);
        flash_initialized = true;
    }
    uni_hal_core_irq_resume(mask);
    if (candidate != nullptr) {
        vSemaphoreDelete(candidate);
    }
    return true;
}

void uni_hal_flash_init(void) {
    // Cortex-M FreeRTOS critical sections can leave BASEPRI raised before scheduler start.
    (void)prepare_flash(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
}


bool uni_hal_flash_configure_latency_auto(uint32_t axi_clock_hz)
{
    uint32_t const effective_axi_clock_hz = axi_clock_hz != 0U ? axi_clock_hz : _uni_hal_flash_calc_axi_clock_hz();
    if (effective_axi_clock_hz == 0U) {
        return false;
    }

    uint32_t const vos = LL_PWR_GetRegulVoltageScaling();
    uint32_t const vos_index = _uni_hal_flash_vos_to_index(vos);
    if (vos_index == UINT32_MAX) {
        return false;
    }

    uint32_t wrhighfreq = 0U;
    uint32_t const latency = _uni_hal_flash_select_latency(vos_index, effective_axi_clock_hz, &wrhighfreq);
    if (latency == UINT32_MAX) {
        return false;
    }

    return _uni_hal_flash_apply_latency(latency, wrhighfreq);
}

// One owner covers the full public operation, including multi-sector or multi-word calls.
static bool claim_flash(void) {
    if (__get_IPSR() != 0U || __get_PRIMASK() != 0U || __get_BASEPRI() != 0U || __get_FAULTMASK() != 0U) {
        return false;
    }
    uint32_t const mask = uni_hal_core_irq_pause();
    bool const available = !flash_owned && sector_erase.state == ERASE_IDLE && controller_idle();
    if (available) {
        flash_owned = true;
    }
    uni_hal_core_irq_resume(mask);
    if (available && ((relock_options && !lock_options()) || (relock_control && !lock_flash()))) {
        flash_owned = false;
        return false;
    }
    return available;
}

size_t uni_hal_flash_write(size_t addr, size_t size, uint8_t *src)
{
    uint32_t flash_word[FLASH_NB_32BITWORD_IN_FLASHWORD];
    size_t bytes_written = 0;

    if (src == nullptr || size == 0 || size % sizeof(flash_word) != 0 || addr < FLASH_BASE || addr >= FLASH_END) {
        return 0;
    }

    if (addr % sizeof(flash_word) != 0 || size > (FLASH_END - addr) + 1U || !claim_flash()) {
        return 0;
    }

    if (HAL_FLASH_Unlock() == HAL_OK) {
        while (bytes_written < size) {
            // ST reads uint32_t values. Use byte loads even when Cortex-M unaligned access traps are enabled.
            const volatile uint8_t *source = src + bytes_written;
            for (size_t i = 0; i < sizeof(flash_word); ++i) {
                ((uint8_t *)flash_word)[i] = source[i];
            }
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, addr + bytes_written, (uint32_t)flash_word) != HAL_OK) {
                break;
            }
            bytes_written += sizeof(flash_word);
        }
    }
    // Unlock can leave the first bank open when the second bank rejects its key.
    (void)lock_flash();
    flash_owned = false;
    return bytes_written;
}

static void poll_sector(uint32_t tick_hz) {
    uint32_t const start = uni_hal_systick_get_ms();
    uint32_t last = start;
    // Allow at least two full tick periods without a time change. Even a stopped
    // or misconfigured SysTick cannot strand the caller in this loop.
    uint32_t const max_spins = 2U * (SystemCoreClock / (tick_hz < 1000U ? tick_hz : 1000U) + 1U);
    uint32_t spins = max_spins;
    while (sector_erase.state == ERASE_WAITING) {
        uint32_t const now = uni_hal_systick_get_ms();
        if (now - start >= UNI_HAL_FLASH_ERASE_TIMEOUT_MS || --spins == 0U) {
            break;
        }
        if (now != last) {
            last = now;
            spins = max_spins;
        }
    }
}

static bool erase_one_sector(uni_hal_flash_bank_e bank, uni_hal_flash_sector_e sector) {
    BaseType_t const scheduler = xTaskGetSchedulerState();
    sector_erase.notify_task = scheduler == taskSCHEDULER_RUNNING;
    uint32_t const tick_mask = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk;
    if (!NVIC_GetEnableIRQ(FLASH_IRQn) ||
        (!sector_erase.notify_task && (SysTick->CTRL & tick_mask) != tick_mask)) {
        return false;
    }
    if (sector_erase.notify_task && !prepare_flash(true)) {
        return false;
    }
    if (HAL_FLASH_Unlock() != HAL_OK) {
        (void)lock_flash();
        return false;
    }
    FLASH_EraseInitTypeDef ei = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Banks = bank == UNI_HAL_FLASH_BANK_2 ? FLASH_BANK_2 : FLASH_BANK_1,
        .Sector = (uint32_t)sector,
        .NbSectors = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_4
    };
    sector_erase.bank = bank;
    // RM0433: bank swapping remaps both the AXI addresses and the CR1/CR2 registers.
    uintptr_t const base = bank == UNI_HAL_FLASH_BANK_2 ? FLASH_BANK2_BASE : FLASH_BANK1_BASE;
    sector_erase.address = base + (uint32_t)sector * FLASH_SECTOR_SIZE;
    if (sector_erase.notify_task) {
        // A completion concurrent with the previous wait's expiry can leave its token.
        (void)xSemaphoreTake(xFlashDoneSem, 0);
    }
    sector_erase.state = ERASE_WAITING;
    if (HAL_FLASHEx_Erase_IT(&ei) != HAL_OK) {
        (void)lock_flash();
        sector_erase.state = ERASE_IDLE;
        return false;
    }

    if (sector_erase.notify_task) {
        (void)xSemaphoreTake(xFlashDoneSem, pdMS_TO_TICKS(UNI_HAL_FLASH_ERASE_TIMEOUT_MS));
    } else {
        poll_sector(scheduler == taskSCHEDULER_NOT_STARTED ? 1000U : configTICK_RATE_HZ);
    }
    uint32_t const mask = uni_hal_core_irq_pause();
    flash_erase_state_t const result = sector_erase.state;
    bool const done = result == ERASE_OK || result == ERASE_FAILED;
    if (!done) {
        sector_erase.state = ERASE_TIMED_OUT;
    }
    uni_hal_core_irq_resume(mask);
    if (!done) {
        return false;
    }
    bool const locked = finish_erase();
    sector_erase.state = ERASE_IDLE;
    return locked && result == ERASE_OK;
}

bool uni_hal_flash_erase_sector(uni_hal_flash_bank_e bank, uni_hal_flash_sector_e sector) {
    if (!flash_initialized || (unsigned)bank > UNI_HAL_FLASH_BANK_2 ||
        (unsigned)sector >= FLASH_SECTOR_TOTAL || !claim_flash()) {
        return false;
    }
    bool const result = erase_one_sector(bank, sector);
    flash_owned = false;
    return result;
}

bool uni_hal_flash_erase_bank(uni_hal_flash_bank_e bank) {
    if (!flash_initialized || (unsigned)bank > UNI_HAL_FLASH_BANK_2 || !claim_flash()) {
        return false;
    }
    bool result = true;
    for (uint32_t sector = 0U; result && sector < FLASH_SECTOR_TOTAL; ++sector) {
        result = erase_one_sector(bank, (uni_hal_flash_sector_e)sector);
    }
    flash_owned = false;
    return result;
}


bool uni_hal_flash_erase(size_t addr, size_t size)
{
    HAL_StatusTypeDef status;
    uint32_t sectorError = 0;

    if (size == 0 || addr < FLASH_BASE || addr > FLASH_END) {
        return 0;
    }

    // sector numbers restart from 0 in each bank
    size_t const bank_base = (addr < FLASH_BANK2_BASE) ? FLASH_BANK1_BASE : FLASH_BANK2_BASE;
    size_t const offset = addr - bank_base;
    if (size > (FLASH_SECTOR_TOTAL * FLASH_SECTOR_SIZE) - offset || !claim_flash()) {
        return 0;
    }

    size_t start_sector = offset / FLASH_SECTOR_SIZE;
    size_t nb_sector = ((offset + size - 1U) / FLASH_SECTOR_SIZE) - start_sector + 1U;

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        (void)lock_flash();
        flash_owned = false;
        return false;
    }

    FLASH_EraseInitTypeDef eraseInitStruct = {0};

    eraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInitStruct.Banks = (addr < FLASH_BANK2_BASE) ? FLASH_BANK_1 : FLASH_BANK_2;
    eraseInitStruct.Sector = start_sector;
    eraseInitStruct.NbSectors = nb_sector;
    eraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    status = HAL_FLASHEx_Erase(&eraseInitStruct, &sectorError);
    bool const locked = lock_flash();
    flash_owned = false;
    return locked && status == HAL_OK && sectorError == UINT32_MAX;
}


bool uni_hal_flash_swap_banks(void)
{
    if (!claim_flash()) {
        return false;
    }
    bool result = false;

    bool const suspend = xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    if (suspend) {
        vTaskSuspendAll();
    }
    if (HAL_FLASH_Unlock() == HAL_OK && HAL_FLASH_OB_Unlock() == HAL_OK)
    {
        FLASH_OBProgramInitTypeDef OBInit = {0};

        OBInit.Banks = FLASH_BANK_1;
        HAL_FLASHEx_OBGetConfig(&OBInit);

        OBInit.OptionType = OPTIONBYTE_USER;
        OBInit.USERType   = OB_USER_SWAP_BANK;
        if ((OBInit.USERConfig & OB_SWAP_BANK_ENABLE) == OB_SWAP_BANK_DISABLE) {
            OBInit.USERConfig = OB_SWAP_BANK_ENABLE;
        } else {
            OBInit.USERConfig = OB_SWAP_BANK_DISABLE;
        }

        result = HAL_FLASHEx_OBProgram(&OBInit) == HAL_OK;
        result = result && HAL_FLASH_OB_Launch() == HAL_OK;
        if (result)
        {
            SCB_InvalidateICache();
            NVIC_SystemReset();
        }
    }

    // unreachable on success
    (void)lock_options();
    (void)lock_flash();
    flash_owned = false;
    if (suspend) {
        (void)xTaskResumeAll();
    }
    return result;
}

// The real H743 driver sees controlled HAL IRQs, scheduler interleavings and cache maintenance.
#include <stm32h7xx_hal.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include "flash/uni_hal_flash.h"
#include "systick/uni_hal_systick.h"
#include "../arm/semihost_test.h"

static FLASH_TypeDef regs;
static SCB_Type scb;
static SysTick_Type tick;
static bool irq_enabled = true;
#undef SysTick
#define SysTick (&tick)
#undef NVIC_GetEnableIRQ
#define NVIC_GetEnableIRQ(irq) irq_enabled
#define SystemCoreClock 8000U
#undef FLASH
#define FLASH (&regs)
#undef SCB
#define SCB (&scb)

static uint32_t now, erase_calls, writes, ranges, unlocks, locks, wakes;
static uint32_t cache_addr, cache_size, icache_flushes, dcache_flushes;
static uint8_t word[32] __attribute__((aligned(32)));
static bool signalled, pending, unlocked, nested_ok, reject_unlock, fail_allocation;
static unsigned allocations, kernel_calls_before_start, suspends, resumes, ob_programs, ob_launches;
static bool reject_option_program, reject_lock, reject_option_lock, option_timeout;
static BaseType_t model_scheduler = taskSCHEDULER_RUNNING;
static uni_hal_flash_bank_e active_bank;
enum event { EVENT_SUCCESS, EVENT_ERROR, EVENT_TIMEOUT, EVENT_FINISH_ON_TIMEOUT, EVENT_COMPETE, EVENT_CHECK_BANK };
static enum event action;
static bool irq_event, inject_error, test_polling, frozen_clock;

void FLASH_IRQHandler(void);
static void finish(bool error) {
    pending = false;
    if (active_bank == UNI_HAL_FLASH_BANK_1) regs.SR1 = 0; else regs.SR2 = 0;
    irq_event = true;
    inject_error = error;
    FLASH_IRQHandler();
}
static HAL_StatusTypeDef unlock(void) {
    ++unlocks;
    unlocked = true; // The first bank accepts its key even when the second bank rejects it.
    if (reject_unlock) return HAL_ERROR;
    return HAL_OK;
}
static HAL_StatusTypeDef lock(void) {
    ++locks;
    if (reject_lock) return HAL_ERROR;
    unlocked = false;
    return HAL_OK;
}
static HAL_StatusTypeDef begin(FLASH_EraseInitTypeDef *ei) {
    if (pending || !unlocked) return HAL_BUSY;
    ++erase_calls;
    active_bank = ei->Banks == FLASH_BANK_1 ? UNI_HAL_FLASH_BANK_1 : UNI_HAL_FLASH_BANK_2;
    if (active_bank == UNI_HAL_FLASH_BANK_1) regs.SR1 = FLASH_SR_QW; else regs.SR2 = FLASH_SR_QW;
    pending = true;
    return HAL_OK;
}
static bool all_competitors_rejected(void) {
    static bool checking;
    if (checking) return true;
    checking = true;
    unsigned const prior = unlocks;
    bool const ok = uni_hal_flash_write(FLASH_BASE, sizeof word, word) == 0U &&
           !uni_hal_flash_erase(FLASH_BASE, 1) &&
           !uni_hal_flash_erase_bank(UNI_HAL_FLASH_BANK_1) &&
           !uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_2, UNI_HAL_FLASH_SECTOR_1) &&
           !uni_hal_flash_swap_banks() && prior == unlocks;
    checking = false;
    return ok;
}
static void handle_irq(void) {
    if (!irq_event) return;
    irq_event = false;
    HAL_FLASH_EndOfOperationCallback(UINT32_MAX);
    if (inject_error) HAL_FLASH_OperationErrorCallback(1);
    // The vendor still owns pFlash in its callbacks and only unlocks it at IRQ exit.
    nested_ok = all_competitors_rejected() && nested_ok;
}
static SemaphoreHandle_t create_event(void) {
    ++allocations;
    if (model_scheduler != taskSCHEDULER_RUNNING) ++kernel_calls_before_start;
    return fail_allocation ? nullptr : (SemaphoreHandle_t)&regs;
}
static BaseType_t take(TickType_t ticks) {
    if (model_scheduler != taskSCHEDULER_RUNNING) ++kernel_calls_before_start;
    if (ticks && pending) {
        if (model_scheduler != taskSCHEDULER_RUNNING) nested_ok = false;
        if (action == EVENT_TIMEOUT) return pdFALSE;
        if (action == EVENT_COMPETE || action == EVENT_CHECK_BANK)
            nested_ok = nested_ok && all_competitors_rejected();
        finish(action == EVENT_ERROR);
        if (action == EVENT_COMPETE) nested_ok = nested_ok && all_competitors_rejected();
        if (action == EVENT_FINISH_ON_TIMEOUT) return pdFALSE;
    }
    if (!signalled) return pdFALSE;
    signalled = false;
    return pdTRUE;
}
static BaseType_t give(BaseType_t *woken) {
    if (model_scheduler != taskSCHEDULER_RUNNING) ++kernel_calls_before_start;
    ++wakes;
    signalled = true;
    *woken = pdTRUE;
    return pdTRUE;
}
static uint32_t milliseconds(void) {
    if (!frozen_clock) now += 10000U;
    if (test_polling && pending && action != EVENT_TIMEOUT) finish(false);
    return now;
}
static HAL_StatusTypeDef program(void) {
    ++writes;
    nested_ok = nested_ok && all_competitors_rejected();
    return HAL_OK;
}
static HAL_StatusTypeDef erase(uint32_t *error) {
    ++ranges;
    nested_ok = nested_ok && all_competitors_rejected();
    *error = UINT32_MAX;
    return HAL_OK;
}
static HAL_StatusTypeDef program_options(void) {
    ++ob_programs;
    nested_ok = nested_ok && all_competitors_rejected();
    return reject_option_program ? HAL_ERROR : HAL_OK;
}
static HAL_StatusTypeDef launch_options(void) {
    ++ob_launches;
    if (option_timeout) {
        regs.OPTSR_CUR |= FLASH_OPTSR_OPT_BUSY;
        return HAL_TIMEOUT;
    }
    return HAL_ERROR;
}
static inline void invalidate(volatile void *addr, int32_t size) {
    ++dcache_flushes;
    cache_addr = (uintptr_t)addr;
    cache_size = (uint32_t)size;
}
#undef xSemaphoreCreateBinary
#undef vSemaphoreDelete
#define vSemaphoreDelete(handle) ((void)(handle))
#undef xSemaphoreTake
#undef xSemaphoreGiveFromISR
#define xSemaphoreCreateBinary() create_event()
#define xSemaphoreTake(handle, ticks) take(ticks)
#define xSemaphoreGiveFromISR(handle, woken) give(woken)
#define HAL_FLASH_IRQHandler handle_irq
#define HAL_FLASHEx_Erase_IT begin
#define HAL_FLASH_Unlock unlock
#define HAL_FLASH_Lock lock
#define HAL_FLASH_Program(type, addr, src) ((void)(type), (void)(addr), (void)(src), program())
#define HAL_FLASHEx_Erase(ei, error) ((void)(ei), erase(error))
#define HAL_FLASH_OB_Unlock() HAL_OK
#define HAL_FLASHEx_OBGetConfig(options) ((void)(options))
#define HAL_FLASHEx_OBProgram(options) ((void)(options), program_options())
#define HAL_FLASH_OB_Launch() launch_options()
#define HAL_FLASH_OB_Lock() (reject_option_lock ? HAL_ERROR : HAL_OK)
#define HAL_NVIC_SetPriority(irq, priority, group) ((void)0)
#define HAL_NVIC_EnableIRQ(irq) ((void)0)
#define xTaskGetSchedulerState() (model_scheduler)
#define uni_hal_systick_get_ms() milliseconds()
#define SCB_CleanInvalidateDCache() ((void)0)
#define SCB_DisableICache() (scb.CCR &= ~SCB_CCR_IC_Msk)
#define SCB_DisableDCache() (scb.CCR &= ~SCB_CCR_DC_Msk)
#define SCB_EnableICache() (scb.CCR |= SCB_CCR_IC_Msk)
#define SCB_EnableDCache() (scb.CCR |= SCB_CCR_DC_Msk)
#define SCB_InvalidateICache() (++icache_flushes)
#define SCB_InvalidateDCache_by_Addr(addr, size) invalidate(addr, size)
#define vTaskSuspendAll() (++suspends)
#define xTaskResumeAll() (++resumes, pdFALSE)
#include "os/uni_hal_os.h"
#undef UNI_HAL_OS_ISR_EXIT
#define UNI_HAL_OS_ISR_EXIT(woken) ((void)(woken))
#include "flash/uni_hal_flash_stm32h7.c"

static unsigned check(void) {
    scb.CCR = SCB_CCR_IC_Msk | SCB_CCR_DC_Msk;
    model_scheduler = taskSCHEDULER_NOT_STARTED;
    uni_hal_flash_init();
    CHECK(allocations == 0);
    nested_ok = true;
    model_scheduler = taskSCHEDULER_RUNNING;
    fail_allocation = true;
    CHECK(!uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_0) && unlocks == 0);
    fail_allocation = false;
    action = EVENT_COMPETE;
    tick.CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk;
    CHECK(uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_2, UNI_HAL_FLASH_SECTOR_2));
    CHECK(nested_ok && erase_calls == 1 && writes == 0 && ranges == 0 && !unlocked);
    CHECK(dcache_flushes == 1 && cache_addr == FLASH_BANK2_BASE + 2 * FLASH_SECTOR_SIZE &&
          cache_size == FLASH_SECTOR_SIZE && icache_flushes == 1);

    action = EVENT_TIMEOUT;
    CHECK(!uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_0));
    CHECK(pending && all_competitors_rejected() && dcache_flushes == 1);
    regs.SR1 = 0; // Controller already idle, completion vector still delayed.
    CHECK(all_competitors_rejected());
    CHECK(scb.CCR == (SCB_CCR_IC_Msk | SCB_CCR_DC_Msk));
    FLASH_IRQHandler(); // A vector without terminal status is not a completion.
    CHECK(pending && all_competitors_rejected());
    finish(true);
    CHECK(!unlocked && wakes == 1 && dcache_flushes == 2);
    // A second timeout ends in an IRQ whose lock write fails. No new unlock until both banks relock.
    CHECK(!uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_0));
    reject_lock = true;
    finish(false);
    CHECK(unlocked && all_competitors_rejected());
    reject_lock = false;
    action = EVENT_FINISH_ON_TIMEOUT;
    CHECK(uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_1));
    action = EVENT_ERROR;
    CHECK(!uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_2));
    CHECK(!pending && !unlocked);

    action = EVENT_CHECK_BANK;
    unsigned const before = erase_calls;
    CHECK(uni_hal_flash_erase_bank(UNI_HAL_FLASH_BANK_1));
    CHECK(erase_calls == before + FLASH_SECTOR_TOTAL && nested_ok);
    CHECK(uni_hal_flash_write(FLASH_BASE, sizeof word, word) == sizeof word && nested_ok);
    CHECK(uni_hal_flash_erase(FLASH_BASE, 1) && nested_ok);

    reject_option_program = true;
    CHECK(!uni_hal_flash_swap_banks());
    CHECK(ob_programs == 1 && ob_launches == 0 && suspends == 1 && resumes == 1);
    CHECK(!unlocked && uni_hal_flash_write(FLASH_BASE, sizeof word, word) == sizeof word && nested_ok);
    reject_option_program = false;
    CHECK(!uni_hal_flash_swap_banks());
    CHECK(ob_programs == 2 && ob_launches == 1 && suspends == 2 && resumes == 2);
    CHECK(!unlocked && uni_hal_flash_write(FLASH_BASE, sizeof word, word) == sizeof word && nested_ok);

    option_timeout = true;
    CHECK(!uni_hal_flash_swap_banks());
    CHECK((regs.OPTSR_CUR & FLASH_OPTSR_OPT_BUSY) != 0U && all_competitors_rejected());
    regs.OPTSR_CUR = 0U;
    reject_option_lock = true;
    CHECK(all_competitors_rejected());
    reject_option_lock = false;
    CHECK(uni_hal_flash_write(FLASH_BASE, sizeof word, word) == sizeof word && nested_ok);
    option_timeout = false;

    test_polling = true;
    model_scheduler = taskSCHEDULER_NOT_STARTED;
    CHECK(!uni_hal_flash_swap_banks() && suspends == 3 && resumes == 3);
    CHECK(!unlocked && uni_hal_flash_write(FLASH_BASE, sizeof word, word) == sizeof word && nested_ok);
    unsigned const task_wakes = wakes;
    scb.CCR = 0;
    unsigned const cache_count = dcache_flushes;
    action = EVENT_SUCCESS;
    CHECK(uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_0));
    CHECK(wakes == task_wakes && scb.CCR == 0 && dcache_flushes == cache_count);
    action = EVENT_TIMEOUT;
    CHECK(!uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_0));
    CHECK(pending && all_competitors_rejected());
    finish(false);
    model_scheduler = taskSCHEDULER_SUSPENDED;
    action = EVENT_SUCCESS;
    CHECK(uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_0) && nested_ok);
    CHECK(kernel_calls_before_start == 0);
    irq_enabled = false;
    CHECK(!uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_0) && !pending);
    irq_enabled = true;
    tick.CTRL = 0;
    CHECK(!uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_0) && !pending);
    tick.CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk;
    action = EVENT_TIMEOUT;
    frozen_clock = true;
    CHECK(!uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_0));
    CHECK(pending && all_competitors_rejected());
    finish(false);
    action = EVENT_SUCCESS;
    regs.OPTCR = FLASH_OPTCR_SWAP_BANK;
    scb.CCR = SCB_CCR_DC_Msk;
    CHECK(uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_2, UNI_HAL_FLASH_SECTOR_3));
    CHECK(cache_addr == FLASH_BANK2_BASE + 3U * FLASH_SECTOR_SIZE);
    reject_unlock = true;
    CHECK(!uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_0) && !pending);
    CHECK(!unlocked && !uni_hal_flash_erase(FLASH_BASE, 1) && !unlocked);
    CHECK(uni_hal_flash_write(FLASH_BASE, sizeof word, word) == 0U && !unlocked);
    reject_unlock = false;
    CHECK(uni_hal_flash_erase_sector(UNI_HAL_FLASH_BANK_1, UNI_HAL_FLASH_SECTOR_0));
    return 0;
}

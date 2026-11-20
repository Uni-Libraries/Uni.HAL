#include <stm32h7xx_hal.h>
#include "flash/uni_hal_flash.h"
#include "../arm/semihost_test.h"

static SCB_Type scb;
#undef SCB
#define SCB (&scb)
static FLASH_TypeDef flash;
#undef FLASH
#define FLASH (&flash)

enum { word_size = FLASH_NB_32BITWORD_IN_FLASHWORD * sizeof(uint32_t) };
static uint8_t bytes[3 * word_size];
static uint32_t const start = FLASH_BANK2_BASE - word_size;
static uint32_t failure_address = UINT32_MAX;
static unsigned attempts, dcache_calls, icache_calls;
static unsigned attempts_at_invalidation;
static uintptr_t invalidated_start;
static size_t invalidated_size;
static bool reject_unlock;

static HAL_StatusTypeDef program(uint32_t type, uint32_t address, uint32_t source) {
    (void)type;
    (void)source;
    ++attempts;
    return address == failure_address ? HAL_ERROR : HAL_OK;
}

static void invalidate_data(void *address, size_t size) {
    ++dcache_calls;
    attempts_at_invalidation = attempts;
    invalidated_start = (uintptr_t)address;
    invalidated_size = size;
}

#define HAL_FLASH_Program program
#define HAL_FLASH_Unlock() (reject_unlock ? HAL_ERROR : HAL_OK)
#define HAL_FLASH_Lock() HAL_OK
#define SCB_InvalidateDCache_by_Addr invalidate_data
#define SCB_InvalidateICache() (++icache_calls)
#include "flash/uni_hal_flash_stm32h7.c"

static unsigned check(void) {
    uint32_t const modes[] = {SCB_CCR_IC_Msk | SCB_CCR_DC_Msk, SCB_CCR_IC_Msk, SCB_CCR_DC_Msk, 0U};
    for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        scb.CCR = modes[i];
        attempts = dcache_calls = icache_calls = 0;
        CHECK(uni_hal_flash_write(start, sizeof(bytes), bytes) == sizeof(bytes));
        CHECK(attempts == 3U);
        CHECK(icache_calls == ((modes[i] & SCB_CCR_IC_Msk) != 0U));
        CHECK(dcache_calls == ((modes[i] & SCB_CCR_DC_Msk) != 0U));
        if (dcache_calls) {
            CHECK(attempts_at_invalidation == attempts);
            CHECK(invalidated_start == start && invalidated_size == sizeof(bytes));
        }
    }

    // HAL can report an error after issuing the store for the second Flash word.
    scb.CCR = SCB_CCR_IC_Msk | SCB_CCR_DC_Msk;
    failure_address = start + word_size;
    attempts = 0;
    CHECK(uni_hal_flash_write(start, sizeof(bytes), bytes) == word_size);
    CHECK(attempts == 2U && icache_calls == 1U && dcache_calls == 1U);
    CHECK(attempts_at_invalidation == attempts);
    CHECK(invalidated_start == start && invalidated_size == 2U * word_size);

    reject_unlock = true;
    CHECK(uni_hal_flash_write(start, sizeof(bytes), bytes) == 0U);
    CHECK(attempts == 2U && icache_calls == 1U && dcache_calls == 1U);
    return 0;
}

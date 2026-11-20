#include <stm32h7xx_hal.h>
#include "flash/uni_hal_flash.h"
#include "../arm/semihost_test.h"

static FLASH_TypeDef flash;
#undef FLASH
#define FLASH (&flash)

static uint32_t programmed[2][FLASH_NB_32BITWORD_IN_FLASHWORD];
static unsigned calls;
static uint32_t const first_address = FLASH_END + 1U - sizeof(programmed);

static HAL_StatusTypeDef program(uint32_t type, uint32_t address, uint32_t source) {
    // HAL_FLASH_Program consumes each row through a 32-bit source pointer.
    if (calls >= 2U || type != FLASH_TYPEPROGRAM_FLASHWORD ||
        address != first_address + calls * sizeof(programmed[0]) || source % alignof(uint32_t)) {
        return HAL_ERROR;
    }
    const volatile uint32_t *words = (const volatile uint32_t *)source;
    for (unsigned i = 0; i < FLASH_NB_32BITWORD_IN_FLASHWORD; ++i) {
        programmed[calls][i] = words[i];
    }
    ++calls;
    return HAL_OK;
}

#define HAL_FLASH_Unlock() HAL_OK
#define HAL_FLASH_Lock() HAL_OK
#define HAL_FLASH_Program program
#include "flash/uni_hal_flash_stm32h7.c"

static unsigned check(void) {
    alignas(4) uint8_t bytes[sizeof(programmed) + 3U];
    for (unsigned i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = (uint8_t)(0xA5U ^ i);
    }
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;
    __DSB();
    __ISB();
    for (unsigned offset = 0; offset < 4U; ++offset) {
        calls = 0;
        CHECK(uni_hal_flash_write(first_address, sizeof(programmed), bytes + offset) == sizeof(programmed));
        CHECK(calls == 2U);
        for (unsigned i = 0; i < sizeof(programmed); ++i) {
            CHECK(((const uint8_t *)&programmed)[i] == bytes[offset + i]);
        }
    }
    return 0;
}

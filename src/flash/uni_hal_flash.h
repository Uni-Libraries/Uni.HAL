#pragma once

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


//
// Typedefs
//

typedef enum {
    UNI_HAL_FLASH_BANK_1,
    UNI_HAL_FLASH_BANK_2,
} uni_hal_flash_bank_e;

typedef enum {
    UNI_HAL_FLASH_SECTOR_0,
    UNI_HAL_FLASH_SECTOR_1,
    UNI_HAL_FLASH_SECTOR_2,
    UNI_HAL_FLASH_SECTOR_3,
    UNI_HAL_FLASH_SECTOR_4,
    UNI_HAL_FLASH_SECTOR_5,
    UNI_HAL_FLASH_SECTOR_6,
    UNI_HAL_FLASH_SECTOR_7,
} uni_hal_flash_sector_e;

//
// Functions
//

void uni_hal_flash_init(void);

size_t uni_hal_flash_get_size(void);

size_t uni_hal_flash_read(size_t addr, size_t size, uint8_t *dst);

size_t uni_hal_flash_write(size_t addr, size_t size, uint8_t *dst);

bool uni_hal_flash_erase(size_t addr, size_t size);

bool uni_hal_flash_erase_bank(uni_hal_flash_bank_e);

bool uni_hal_flash_erase_sector(uni_hal_flash_bank_e bank, uni_hal_flash_sector_e sector);

bool uni_hal_flash_swap_banks(void);

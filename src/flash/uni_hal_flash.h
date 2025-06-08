#pragma once

#include <stddef.h>
#include <stdint.h>

size_t uni_hal_flash_get_size();

size_t uni_hal_flash_read(size_t addr, size_t size, uint8_t *dst);

size_t uni_hal_flash_write(size_t addr, size_t size, uint8_t *dst);

bool uni_hal_flash_erase(size_t addr, size_t size);

bool uni_hal_flash_swap_banks(void);

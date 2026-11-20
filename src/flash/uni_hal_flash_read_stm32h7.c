#include <stm32h7xx.h>
#include <stddef.h>
#include <string.h>

#include "flash/uni_hal_flash.h"

size_t uni_hal_flash_read(size_t addr, size_t size, uint8_t *dst) {
    if (dst == NULL || size == 0 || addr < FLASH_BASE || addr > FLASH_END ||
        size - 1U > FLASH_END - addr) {
        return 0;
    }

    memcpy(dst, (const void *)addr, size);
    return size;
}

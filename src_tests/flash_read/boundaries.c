#undef NDEBUG
#include <assert.h>
#include <string.h>

#include "flash/uni_hal_flash.h"
#include <stm32h7xx.h>

const uint8_t flash_storage[6] = {0xA5, 1, 2, 3, 4, 0x5A};

int main(void) {
    uint8_t dst[4] = {0};
    assert(uni_hal_flash_read(FLASH_BASE, sizeof(dst), dst) == sizeof(dst));
    assert(memcmp(dst, &flash_storage[1], sizeof(dst)) == 0);
    assert(uni_hal_flash_read(FLASH_END, 1, dst) == 1 && dst[0] == 4);

    const uint8_t expected[4] = {4, 2, 3, 4};
    assert(uni_hal_flash_read(FLASH_END - 1, 3, dst) == 0);
    assert(uni_hal_flash_read(FLASH_BASE, SIZE_MAX, dst) == 0);
    assert(uni_hal_flash_read(FLASH_BASE - 1, 1, dst) == 0);
    assert(uni_hal_flash_read(FLASH_END + 1, 1, dst) == 0);
    assert(uni_hal_flash_read(FLASH_BASE, 0, dst) == 0);
    assert(uni_hal_flash_read(FLASH_BASE, 1, nullptr) == 0);
    assert(memcmp(dst, expected, sizeof(dst)) == 0);
    return 0;
}

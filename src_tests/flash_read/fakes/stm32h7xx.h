#pragma once

#include <stdint.h>

// The device header defines FLASH_END as the address of the last accessible byte.
extern const uint8_t flash_storage[6];
#define FLASH_BASE ((uintptr_t)&flash_storage[1])
#define FLASH_END  ((uintptr_t)&flash_storage[4])

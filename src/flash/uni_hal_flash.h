#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

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

/** Initialize the STM32H743/H753 sector-erase IRQ. Repeated calls are safe.
 * Set an NVIC priority grouping compatible with FreeRTOS before calling this function.
 * Before FreeRTOS starts, initialization and erase polling use no kernel objects.
 * Task-mode erases allocate their completion semaphore on first use if necessary.
 */
void uni_hal_flash_init(void);

/** Configure STM32H743/H753 Flash timing for the supplied AXI clock in Hz.
 * Pass zero to derive it from the active system clock, D1 core divider and AHB divider.
 */
bool uni_hal_flash_configure_latency_auto(uint32_t axi_clock_hz);

size_t uni_hal_flash_get_size(void);

size_t uni_hal_flash_read(size_t addr, size_t size, uint8_t *dst);

/**
 * On STM32H743/H753, call mutation APIs from thread mode with interrupts enabled.
 * Writes accept byte-aligned source buffers. The Flash address and size must be multiples of 32 bytes.
 * A competing write, erase or bank swap fails immediately (zero bytes or false).
 */
size_t uni_hal_flash_write(size_t addr, size_t size, uint8_t *src);

bool uni_hal_flash_erase(size_t addr, size_t size);

/** On H743/H753 the whole bank is reserved until completion or the first failing sector. */
bool uni_hal_flash_erase_bank(uni_hal_flash_bank_e);

/**
 * On H743/H753 initialize Flash before erasing a sector or bank. Keep the Flash IRQ
 * and millisecond tick running. A sector time-out returns false. The controller
 * may still be erasing: other mutations are rejected until its final IRQ, which
 * also invalidates the affected cache entries. No other call is needed for cleanup.
 */
bool uni_hal_flash_erase_sector(uni_hal_flash_bank_e bank, uni_hal_flash_sector_e sector);

bool uni_hal_flash_swap_banks(void);

#if defined(__cplusplus)
}
#endif

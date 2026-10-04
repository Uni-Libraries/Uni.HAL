#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdint.h>
#include <stddef.h>



//
// Functions
//

bool uni_hal_core_cm7_dcache_get();
void uni_hal_core_cm7_dcache_set(bool enable);
void uni_hal_core_cm7_dcache_cleaninvalidate_addr(void* ptr, int32_t len);
void uni_hal_core_cm7_dcache_clean(void* ptr, int32_t len);
void uni_hal_core_cm7_dcache_invalidate(void* ptr, int32_t len);

void uni_hal_core_cm7_icache_set(bool enable);

/**
 * Configure the MPU regions.
 * The default sets up the D2 SRAM at 0x30000000 for DMA use: 256 KiB not cacheable, with three
 * 1 KiB device-type regions at 0x30010000, 0x30010400 and 0x30010800 for the Ethernet
 * descriptors and the ADC buffers. It is a weak function: an application whose linker script
 * places these buffers elsewhere provides its own.
 */
void uni_hal_core_cm7_mpu_config(void);
void uni_hal_core_cm7_mpu_set(bool enable);

#if defined(__cplusplus)
}
#endif

#pragma once

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

void uni_hal_core_cm7_icache_set(bool enable);

void uni_hal_core_cm7_mpu_config(void);
void uni_hal_core_cm7_mpu_set(bool enable);

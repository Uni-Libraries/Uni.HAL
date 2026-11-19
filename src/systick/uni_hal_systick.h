#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stdint.h>



//
// Functions
//

bool uni_hal_systick_init(void);

// Monotonic milliseconds modulo 2^32. On STM32 the resolution after startup follows
// the FreeRTOS tick period, with fractional milliseconds carried across ticks.
uint32_t uni_hal_systick_get_ms(void);

uint32_t uni_hal_systick_get_tick(void);

uint32_t uni_hal_systick_get_freq(void);

void uni_hal_systick_delay(uint32_t val);

#if defined(__cplusplus)
}
#endif

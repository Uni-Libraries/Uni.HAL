#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

//stdlib
#include <stdint.h>

//uni.hal
#include "rcc/uni_hal_rcc_enum.h"



//
// Functions
//

/**
 * Route a clock to MCO1 (index 0, PA8) or, on STM32H743/H753, MCO2 (index 1, PC9).
 * clock_divider is 1..15 on H743/H753 and 1, 2, 4, 8 or 16 on L496.
 * Returns 1 after configuring both GPIO and RCC, or 0 on failure.
 */
uint32_t uni_hal_rcc_stm32_mco_enable(uint32_t mco_index, uni_hal_rcc_clksrc_e clock_source, uint32_t clock_divider);

#if defined(__cplusplus)
}
#endif

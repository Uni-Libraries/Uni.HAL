#pragma once

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

uint32_t uni_hal_rcc_stm32_mco_enable(uint32_t mco_index, uni_hal_rcc_clksrc_e clock_source, uint32_t prescaler);

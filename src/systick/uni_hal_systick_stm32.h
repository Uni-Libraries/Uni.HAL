#pragma once

#include <stdbool.h>
#include <stdint.h>

// Internal state shared by the STM32 boot timer, Cortex-M port and RCC.
extern volatile bool uni_hal_systick_uses_rtos_period;
void uni_hal_systick_l4_reclock(uint32_t old_hclk, uint32_t new_hclk);

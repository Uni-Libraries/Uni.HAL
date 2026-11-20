#pragma once

#include "rcc/uni_hal_rcc.h"
#include "rcc/uni_hal_rcc_stm32h7.h"

uni_hal_rcc_stm32h7_config_t model_config(void);
void model_css(void);
unsigned model_test(void);
#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (false)

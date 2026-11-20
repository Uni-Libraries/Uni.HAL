#pragma once

#include "rcc/uni_hal_rcc.h"
#include "rcc/uni_hal_rcc_stm32h7.h"

uni_hal_rcc_stm32h7_config_t model_config(void);
void model_css(void);
unsigned model_test(void);
#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (false)
extern bool model_pll_locks, model_pll_switches, model_inject_css;
extern bool model_css_active, model_flash_allowed;
extern bool model_css_on_claim, model_css_after_claim;
extern unsigned model_clock_reads, model_flash_calls, model_pll_delay;
extern bool model_have_dwt, model_late_pll_switch;
extern uint32_t model_ms_step, model_dwt_step, model_flash_hz;
extern SysTick_Type model_systick;
extern RCC_TypeDef model_rcc;
bool model_hsi_enabled(void);
void model_divide_hsi(void);
void model_change_ahb_divider(void);
uint32_t model_requested_clock(void);
void model_complete_clock_switch(void);

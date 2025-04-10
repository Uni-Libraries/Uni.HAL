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

// uni_hal
#include "core/uni_hal_core.h"
#include "rcc/uni_hal_rcc_enum.h"

#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    #include "rcc/uni_hal_rcc_stm32l4.h"
#elif defined(UNI_HAL_TARGET_MCU_STM32H743)
    #include "rcc/uni_hal_rcc_stm32h7.h"
#else
    #error "unknown target"
#endif



//
// Functions
//

bool uni_hal_rcc_init(void);

bool uni_hal_rcc_is_inited(void);

uint8_t uni_hal_rcc_get_status_reg(void);

bool uni_hal_rcc_get_status_reg_flag(uint8_t flag);



/**
 * Reset the selected peripheral
 * @param ctx RCC context
 * @param target Reset target
 * @return true on success
 */
bool uni_hal_rcc_reset(uni_hal_rcc_reset_target_e target);

//
// Clock
//

/**
 * Get peripheral clock state
 * @param ctx RCC context
 * @param target Peripheral target
 * @return true in case of enabled clock, false in case of disabled or error
 */
bool uni_hal_rcc_clk_get(uni_hal_core_periph_e target);

/**
 * Get peripherial clock frequency
 * @param ctx RCC context
 * @param target peripheral target
 * @return frequency in Hertz
 */
uint32_t uni_hal_rcc_clk_get_freq(uni_hal_core_periph_e target);

/**
 * Set peripheral clock state
 * @param ctx RCC context
 * @param target peripheral target
 * @param state true to enable clock, false to disable clock
 * @return true in case of success, false on error
 */
bool uni_hal_rcc_clk_set(uni_hal_core_periph_e target, bool state);



//
// Clock Source
//

/**
 * Get clock source for the peripheral
 * @param ctx RCC context
 * @param target Peripheral target
 * @return clock source for the target peripheral
 */
uni_hal_rcc_clksrc_e uni_hal_rcc_clksrc_get(uni_hal_core_periph_e target);


/**
 * Set clock source for the peripheral
 * @param ctx RCC context
 * @param target Peripheral target
 * @param source Clock source
 * @return true on succes
 */
bool uni_hal_rcc_clksrc_set(uni_hal_core_periph_e target, uni_hal_rcc_clksrc_e source);


#if defined(__cplusplus)
}
#endif

#pragma once

//
// Includes
//

// stdlib
#include <stdint.h>

//
// Typedefs
//

typedef enum {
    UNI_HAL_CORE_STM32H7_REV_Y = 0x1003, //4099
    UNI_HAL_CORE_STM32H7_REV_B = 0x2000, //8192
    UNI_HAL_CORE_STM32H7_REV_X = 0x2001, //8193
    UNI_HAL_CORE_STM32H7_REV_V = 0x2003, //8195
} uni_hal_core_stm32h7_revision_e;



//
// Functions
//

uni_hal_core_stm32h7_revision_e uni_hal_core_stm32h7_revision_get(void);

uint32_t uni_hal_core_stm32h7_uid_0(void);
uint32_t uni_hal_core_stm32h7_uid_1(void);
uint32_t uni_hal_core_stm32h7_uid_2(void);

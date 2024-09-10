//
// Includes
//

// CMSIS
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    #include <stm32l4xx.h>
#elif defined(UNI_HAL_TARGET_MCU_STM32H743)
    #include <stm32h7xx.h>
#else
    #error "unknown MCU"
#endif

// UNI_HAL
#include "pwr/uni_hal_pwr.h"



//
// Functions
//

void uni_hal_pwr_reset(void) {
    NVIC_SystemReset();
}

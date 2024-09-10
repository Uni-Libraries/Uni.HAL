//
// Includes
//

// stdlib
#include <stddef.h>

// ST
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    #include <stm32l4xx_ll_pwr.h>
#elif defined(UNI_HAL_TARGET_MCU_STM32H743  )
    #include <stm32h7xx_ll_pwr.h>
#else
    #error "unknown MCU"
#endif



// Uni.HAL
#include "pwr/uni_hal_pwr_stm32.h"



//
// Public
//

void uni_hal_pwr_stm_set_backup_access(bool val) {
    val ? LL_PWR_EnableBkUpAccess() : LL_PWR_DisableBkUpAccess();
}

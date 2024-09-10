#pragma once

//
// Typedefs
//

typedef enum {
    UNI_HAL_RCC_CLKSRC_UNKNOWN     = 0,
    UNI_HAL_RCC_CLKSRC_NONE        = 1,
    UNI_HAL_RCC_CLKSRC_CSI         = 2,
    UNI_HAL_RCC_CLKSRC_LSE         = 3,
    UNI_HAL_RCC_CLKSRC_LSI         = 4,
    UNI_HAL_RCC_CLKSRC_HSE         = 5,
    UNI_HAL_RCC_CLKSRC_HSE_DIV_32  = 6,
    UNI_HAL_RCC_CLKSRC_HSI         = 7,
    UNI_HAL_RCC_CLKSRC_HSI48       = 8,
    UNI_HAL_RCC_CLKSRC_PCLK1       = 9,
    UNI_HAL_RCC_CLKSRC_PCLK2       = 10,
    UNI_HAL_RCC_CLKSRC_PCLK4       = 11,
    UNI_HAL_RCC_CLKSRC_PLL1P       = 12,
    UNI_HAL_RCC_CLKSRC_PLL1Q       = 13,
    UNI_HAL_RCC_CLKSRC_PLL2P       = 14,
    UNI_HAL_RCC_CLKSRC_PLL2Q       = 15,
    UNI_HAL_RCC_CLKSRC_PLL3P       = 16,
    UNI_HAL_RCC_CLKSRC_PLL3Q       = 17,
    UNI_HAL_RCC_CLKSRC_PLL3R       = 18,
    UNI_HAL_RCC_CLKSRC_I2S_CKIN    = 19,
    UNI_HAL_RCC_CLKSRC_CLKP        = 20,
    UNI_HAL_RCC_CLKSRC_SYSCLK      = 21,
} uni_hal_rcc_clksrc_e;


typedef enum {
    UNI_HAL_RCC_RESET_BACKUP = 1
} uni_hal_rcc_reset_target_e;

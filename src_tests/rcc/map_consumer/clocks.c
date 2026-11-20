#define uni_hal_rcc_stm32_mco_enable(index, source, divider) ((void)0)

void configure(void) {
#if defined(DIRECTORY_CONFIG) && defined(BOARD_C) && BOARD_REV == 1
    #if defined(DIAGNOSTIC_OUTPUT)
        uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_HSI, 4);
    #else
        uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_HSE, 2);
    #endif
#else
    uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_LSI, 8);
#endif
}

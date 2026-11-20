#define BASE_FREQ 4000000UL
#define HSE_THRESHOLD (BASE_FREQ)
/* #if 0
#endif
*/
#if HSE_VALUE > HSE_THRESHOLD /* fast board */
    #if !defined(SLOW) && (HSE_VALUE >= 8000000U)
        __HAL_RCC_GPIOA_CLK_ENABLE();
    #endif
    uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_HSI, 4);
#elif defined HSE_VALUE && HSE_VALUE == HSE_THRESHOLD
    uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_HSE, 2);
#elif HSE_VALUE < HSE_THRESHOLD && \
    HSE_VALUE != 0 && 0U <= HSE_VALUE
    uni_hal_rcc_stm32_mco_enable(1, UNI_HAL_RCC_CLKSRC_LSI, 3);
#endif
#undef HSE_THRESHOLD
#if defined(HSE_THRESHOLD)
    __HAL_RCC_GPIOB_CLK_ENABLE();
#endif

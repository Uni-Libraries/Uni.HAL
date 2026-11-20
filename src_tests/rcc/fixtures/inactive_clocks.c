#if 0
const uni_hal_rcc_config_t clocks = {
    .hse_enable = true,
    .pll[0] = { .m = 2, .n = 16, .p = 4 },
};
uni_hal_spi_context_t spi = { .instance = SPI1, .clock_source = RCC_PLLSOURCE_HSE };
uni_hal_usart_context_t uart = { .instance = USART1, .clksrc = RCC_PLLSOURCE_HSE };
uni_hal_i2c_context_t i2c = { .instance = I2C1, .clock_source = RCC_PLLSOURCE_HSE };
uni_hal_adc_context_t adc = { .instance = ADC1, .clock_source = RCC_PLLSOURCE_HSE };
void configure(void) {
    LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_8);
    uni_hal_rcc_stm32_mco_enable(1, UNI_HAL_RCC_CLKSRC_HSE, 2);
}
#else
void configure(void) {
    uni_hal_rcc_stm32_mco_enable(0, UNI_HAL_RCC_CLKSRC_HSI, 4);
}
#endif

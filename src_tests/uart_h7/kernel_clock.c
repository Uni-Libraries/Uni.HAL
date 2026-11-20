#include "clock_model.h"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    uint32_t const priority = configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS);
    UNI_HAL_IO_DEFINITION(first, 8, 8);
    UNI_HAL_IO_DEFINITION(second, 8, 8);
    UNI_HAL_IO_DEFINITION(third, 8, 8);
    uni_hal_gpio_pin_context_t pin = {};
    uni_hal_usart_context_t ctx = {.instance = UNI_HAL_CORE_PERIPH_UART_1, .io = &first_ctx,
        .pin_rx = &pin, .pin_tx = &pin, .baudrate = 125000U, .isr_priority = priority, .clksrc = UNI_HAL_RCC_CLKSRC_CSI};
    uni_hal_usart_context_t sibling = ctx;
    sibling.instance = UNI_HAL_CORE_PERIPH_UART_6;
    sibling.io = &second_ctx;
    sibling.clksrc = UNI_HAL_RCC_CLKSRC_UNKNOWN;
    uni_hal_usart_context_t apb1 = ctx;
    apb1.instance = UNI_HAL_CORE_PERIPH_UART_2;
    apb1.io = &third_ctx;
    apb1.clksrc = UNI_HAL_RCC_CLKSRC_HSI;
    g_uni_hal_rcc_status.inited = true;
    rcc.CR = RCC_CR_CSIRDY | RCC_CR_HSIRDY;
    usart1.ISR = usart2.ISR = usart6.ISR = USART_ISR_TEACK | USART_ISR_REACK;

    CHECK(uni_hal_usart_init(&ctx));
    CHECK(LL_RCC_GetUSARTClockSource(LL_RCC_USART16_CLKSOURCE) == LL_RCC_USART16_CLKSOURCE_CSI);
    CHECK(uni_hal_usart_init(&sibling) && uni_hal_usart_init(&apb1));
    CHECK(usart1.BRR == CSI_VALUE / ctx.baudrate && usart6.BRR == usart1.BRR);
    CHECK(usart2.BRR == HSI_VALUE / apb1.baudrate && uni_hal_usart_baudrate_get(&ctx) == ctx.baudrate);
    CHECK(uni_hal_rcc_clksrc_get(sibling.instance) == UNI_HAL_RCC_CLKSRC_CSI);
    CHECK(uni_hal_rcc_clksrc_get(apb1.instance) == UNI_HAL_RCC_CLKSRC_HSI);
    CHECK(!uni_hal_rcc_clksrc_set(ctx.instance, UNI_HAL_RCC_CLKSRC_PCLK1));
    CHECK(LL_RCC_GetUSARTClockSource(LL_RCC_USART16_CLKSOURCE) == LL_RCC_USART16_CLKSOURCE_CSI);
    return 0;
}

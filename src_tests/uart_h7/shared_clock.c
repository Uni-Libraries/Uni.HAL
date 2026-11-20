#include "clock_model.h"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    uint32_t const priority = configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS);
    UNI_HAL_IO_DEFINITION(first, 8, 8);
    UNI_HAL_IO_DEFINITION(second, 8, 8);
    uni_hal_gpio_pin_context_t pins[2] = {};
    uni_hal_usart_context_t ctx = {.instance = UNI_HAL_CORE_PERIPH_UART_2, .io = &first_ctx,
        .pin_rx = &pins[0], .pin_tx = &pins[0], .baudrate = 125000U, .isr_priority = priority,
        .clksrc = UNI_HAL_RCC_CLKSRC_HSI};
    uni_hal_usart_context_t peer = ctx;
    peer.instance = UNI_HAL_CORE_PERIPH_UART_3;
    peer.io = &second_ctx;
    peer.pin_rx = peer.pin_tx = &pins[1];
    peer.clksrc = UNI_HAL_RCC_CLKSRC_CSI;
    g_uni_hal_rcc_status.inited = true;
    rcc.CR = RCC_CR_CSIRDY | RCC_CR_HSIRDY;
    usart2.ISR = usart3.ISR = USART_ISR_TEACK | USART_ISR_REACK;

    preempting_port = &peer;
    CHECK(uni_hal_usart_init(&ctx));
    CHECK(!preempting_result && !pins[1].inited && !usart3.CR1);
    CHECK(!uni_hal_usart_init(&peer));
    CHECK(usart2.BRR == HSI_VALUE / ctx.baudrate && uni_hal_usart_baudrate_get(&ctx) == ctx.baudrate);
    peer.clksrc = UNI_HAL_RCC_CLKSRC_HSI;
    CHECK(uni_hal_usart_init(&peer));
    ctx.clksrc = UNI_HAL_RCC_CLKSRC_CSI;
    CHECK(!uni_hal_usart_init(&ctx) && uni_hal_uart_is_inited(&ctx));
    CHECK(usart3.BRR == usart2.BRR && uni_hal_usart_baudrate_get(&peer) == peer.baudrate);

    // A failed init in the independent USART1/6 group must release its clock reservation.
    UNI_HAL_IO_DEFINITION(third, 8, 8);
    uni_hal_usart_context_t apb2 = {.instance = UNI_HAL_CORE_PERIPH_UART_1, .io = &third_ctx,
        .pin_rx = pins, .pin_tx = pins, .baudrate = 125000U, .isr_priority = priority, .clksrc = UNI_HAL_RCC_CLKSRC_PLL2Q};
    CHECK(!uni_hal_usart_init(&apb2)); // PLL2Q is off.
    apb2.instance = UNI_HAL_CORE_PERIPH_UART_6;
    apb2.clksrc = UNI_HAL_RCC_CLKSRC_CSI;
    usart6.ISR = USART_ISR_TEACK | USART_ISR_REACK;
    CHECK(uni_hal_usart_init(&apb2) && usart6.BRR == CSI_VALUE / apb2.baudrate);
    CHECK(uni_hal_usart_baudrate_get(&ctx) == ctx.baudrate);
    return 0;
}

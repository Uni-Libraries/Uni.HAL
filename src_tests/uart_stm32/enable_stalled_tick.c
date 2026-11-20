#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_rcc.h>
#else
#include <stm32l4xx_ll_rcc.h>
#endif
#include "systick/uni_hal_systick.h"
#include "uart/uni_hal_uart.h"
#include "../arm/semihost_test.h"

static USART_TypeDef uart;
#undef USART1
#define USART1 (&uart)
#define LL_RCC_GetUSARTClockFreq(source) ((void)(source), 16000000U)
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_usart.c"
#else
#include "../../3rdparty/st_stm32l4/src_hal/stm32l4xx_ll_usart.c"
#endif

static uint32_t reads, now, step, acknowledge_after;
static bool late_tick;
static uint32_t milliseconds(void) {
    // Release the original unbounded loop so that a regression reports a failed assertion.
    if (++reads > 100000U) {
        late_tick = true;
        return now + 1000U;
    }
    if (reads == acknowledge_after) {
        uart.ISR |= USART_ISR_TEACK | USART_ISR_REACK;
    }
    now += step;
    return now;
}
#define uni_hal_systick_get_ms() milliseconds()
#define uni_hal_rcc_clk_get_freq(port) ((void)(port), 16000000U)
#define uni_hal_rcc_clk_set(port, enabled) ((void)(port), (enabled))
#define uni_hal_gpio_pin_init(pin) ((pin) != nullptr)
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#define LL_USART_DeInit(handle) ((void)(handle))
#include "uart/uni_hal_uart_stm32h7.c"
#else
#include "uart/uni_hal_uart_stm32l4.c"
#endif

#if configAPPLICATION_ALLOCATED_HEAP
alignas(portBYTE_ALIGNMENT) uint8_t ucHeap[configTOTAL_HEAP_SIZE];
#endif

static unsigned check(void) {
    UNI_HAL_IO_DEFINITION(io, 8, 8);
    uni_hal_gpio_pin_context_t pin = {};
    uni_hal_usart_context_t ctx = {.io = &io_ctx, .pin_rx = &pin, .pin_tx = &pin,
        .instance = UNI_HAL_CORE_PERIPH_UART_1, .baudrate = 125000U,
        .isr_priority = configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS)};
    SystemCoreClock = 100000U;
    uart.ISR = USART_ISR_TEACK;
    CHECK(!uni_hal_usart_init(&ctx));
    CHECK(!late_tick && !ctx.initialized && !(uart.CR1 & USART_CR1_UE));

    reads = 0;
    acknowledge_after = 3U;
    CHECK(uni_hal_usart_init(&ctx) && uart.BRR == 128U);
    CHECK(reads == acknowledge_after);
    acknowledge_after = 0;
    uart.ISR = USART_ISR_REACK;
    reads = 0;
    CHECK(!uni_hal_usart_baudrate_set(&ctx, 250000U));
    CHECK(!late_tick);

    // If the millisecond clock is advancing across its wrap, use its normal deadline.
    reads = 0;
    now = UINT32_MAX - 32U;
    step = 8U;
    CHECK(!uni_hal_usart_baudrate_set(&ctx, 250000U));
    CHECK(reads >= 13U && reads <= 15U && !late_tick);

    // A disabled receive direction does not need a receiver acknowledgement.
    CHECK(uni_hal_usart_receive_enable(&ctx, false));
    uart.ISR = USART_ISR_TEACK;
    CHECK(uni_hal_usart_baudrate_set(&ctx, 250000U));
    CHECK(uart.BRR == 64U);
    return 0;
}

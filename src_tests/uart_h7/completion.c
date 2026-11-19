// Run the public transmit path, driver vector and FreeRTOS stream buffers against USART1 in RAM.
#include <stm32h7xx_ll_usart.h>
#include "core/uni_hal_core.h"
#include "uart/uni_hal_uart.h"
static USART_TypeDef uart;
#undef USART1
#define USART1 (&uart)
static unsigned writes, starts, busy_starts, ends, completions;
static bool de;
static void send_byte(USART_TypeDef *dev, uint8_t data) {
    LL_USART_TransmitData8(dev, data);
    dev->ISR &= ~(USART_ISR_TC | USART_ISR_TXE_TXFNF);
    ++writes;
}
static void clear_tc(USART_TypeDef *dev) {
    LL_USART_ClearFlag_TC(dev);
    dev->ISR &= ~USART_ISR_TC;
}
#define LL_USART_TransmitData8 send_byte
#define LL_USART_ClearFlag_TC clear_tc
#define LL_USART_DeInit(dev) ((void)(dev))
#define LL_USART_Init(dev, cfg) ((dev)->ISR |= USART_ISR_TEACK | USART_ISR_REACK, (void)(cfg), SUCCESS)
#define uni_hal_rcc_clk_set(instance, enabled) ((void)(instance), (enabled))
#define uni_hal_rcc_clksrc_set(instance, source) ((void)(instance), (void)(source), true)
#define uni_hal_core_irq_enable(irq, priority, sub) ((void)(irq), (void)(priority), (void)(sub))
#define uni_hal_gpio_pin_init(pin) ((pin) != nullptr)
#include "uart/uni_hal_uart_stm32h7.c"

static void start(void *io, void *cookie) {
    (void)io;
    de = true;
    ++starts;
    if (uni_hal_usart_transmit_busy(cookie)) ++busy_starts;
}
static void end(void *io, void *cookie) { (void)io; (void)cookie; de = false; ++ends; }
static bool completion(void *ctx, void *cookie, uni_hal_usart_callback_e type) {
    (void)ctx; (void)cookie;
    if (type == UNI_HAL_USART_CALLBACK_TC) ++completions;
    return false;
}
#define CHECK(test) do { if (!(test)) return __LINE__; } while (false)

static unsigned check(void) {
    static uint8_t rx[8], tx[8];
    static uni_hal_io_context_t io = {.buf_tx = {.size = sizeof tx, .array = tx},
                                      .buf_rx = {.size = sizeof rx, .array = rx}};
    uni_hal_gpio_pin_context_t pin = {};
    uni_hal_usart_context_t ctx = {.instance = UNI_HAL_CORE_PERIPH_UART_1, .io = &io,
                                   .pin_tx = &pin, .pin_rx = &pin, .baudrate = 115200U};
    CHECK(uni_hal_usart_init(&ctx));
    io.handlers.tx_start = start;
    io.handlers.tx_start_ctx = &ctx;
    io.handlers.tx_end = end;
    CHECK(uni_hal_usart_set_callback(&ctx, completion, nullptr));
    uint8_t first = 1U, second = 2U;

    CHECK(uni_hal_usart_transmit_data(&ctx, &first, 1) == 1U);
    uart.ISR |= USART_ISR_TXE_TXFNF | USART_ISR_TC; // Old TC from an idle line.
    USART1_IRQHandler();
    CHECK(de && starts == 1U && writes == 1U && completions == 0U);
    CHECK(busy_starts == starts);
    CHECK(uni_hal_usart_transmit_busy(&ctx));
    uart.ISR |= USART_ISR_TXE_TXFNF | USART_ISR_TC; // Last frame already finished before TXE was serviced.
    USART1_IRQHandler();
    CHECK(!de && ends == 1U && completions == 1U && !uni_hal_usart_transmit_busy(&ctx));
    USART1_IRQHandler();
    CHECK(completions == 1U);

    CHECK(uni_hal_usart_transmit_data(&ctx, &first, 1) == 1U);
    USART1_IRQHandler();
    uart.ISR |= USART_ISR_TXE_TXFNF; // Still shifting the first frame.
    USART1_IRQHandler();
    CHECK(de && LL_USART_IsEnabledIT_TC(&uart));
    uart.ISR |= USART_ISR_TC;
    CHECK(uni_hal_usart_transmit_data(&ctx, &second, 1) == 1U);
    USART1_IRQHandler();
    CHECK(de && starts == 2U && ends == 1U && uart.TDR == second);
    uart.ISR |= USART_ISR_TXE_TXFNF;
    USART1_IRQHandler();
    CHECK(de && LL_USART_IsEnabledIT_TC(&uart));
    uart.ISR |= USART_ISR_TC;
    // A delayed trigger can run after the IRQ already took the corresponding byte.
    io.handlers.tx_trigger(&io, io.handlers.tx_trigger_ctx);
    USART1_IRQHandler();
    CHECK(!de && completions == 2U && !uni_hal_usart_transmit_busy(&ctx));
    CHECK(uni_hal_usart_transmit_data(&ctx, &first, 1) == 1U);
    USART1_IRQHandler();
    CHECK(de && starts == 3U);
    CHECK(uni_hal_usart_irq_enable(&ctx, false));
    uart.ISR |= USART_ISR_TC | USART_ISR_TXE_TXFNF;
    USART1_IRQHandler();
    CHECK(de && completions == 2U && uni_hal_usart_transmit_busy(&ctx));
    CHECK(uni_hal_usart_irq_enable(&ctx, true));
    USART1_IRQHandler();
    CHECK(!de && completions == 3U && !uni_hal_usart_transmit_busy(&ctx));
    CHECK(busy_starts == starts);
    return 0;
}

extern char _stack_top;
void run_tests(void);
__attribute__((used, section(".test_vectors")))
const uintptr_t vectors[] = {(uintptr_t)&_stack_top, (uintptr_t)run_tests};
void run_tests(void) {
    uint32_t result[] = {0x20026U, check()};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = result;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

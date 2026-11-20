// Exercise the L496 driver vector and FreeRTOS stream buffers with a USART register bank in RAM.
#include <stm32l4xx_ll_lpuart.h>
#include <stm32l4xx_ll_rcc.h>
#include <stm32l4xx_ll_usart.h>
#include "systick/uni_hal_systick.h"
#include "uart/uni_hal_uart.h"

static USART_TypeDef uart;
#ifdef TEST_LPUART
#undef LPUART1
#define LPUART1 (&uart)
#define TEST_INSTANCE UNI_HAL_CORE_PERIPH_LPUART_1
#define test_irq LPUART1_IRQHandler
#define TEST_IRQn LPUART1_IRQn
#else
#undef USART1
#define USART1 (&uart)
#define TEST_INSTANCE UNI_HAL_CORE_PERIPH_UART_1
#define test_irq USART1_IRQHandler
#define TEST_IRQn USART1_IRQn
#endif

static void send_byte(USART_TypeDef *dev, uint8_t data) {
    LL_USART_TransmitData8(dev, data);
    dev->ISR &= ~(USART_ISR_TC | USART_ISR_TXE);
}

static void clear_tc(USART_TypeDef *dev) {
    LL_USART_ClearFlag_TC(dev);
    dev->ISR &= ~USART_ISR_TC;
}

#define LL_USART_TransmitData8 send_byte
#define LL_LPUART_TransmitData8 send_byte
#define LL_USART_ClearFlag_TC clear_tc
#define LL_LPUART_ClearFlag_TC clear_tc
// Use the vendor init, including its check that UE is clear before configuring BRR.
#ifdef TEST_LPUART
#define LL_RCC_GetLPUARTClockFreq(source) ((void)(source), 16000000U)
#include "../../3rdparty/st_stm32l4/src_hal/stm32l4xx_ll_lpuart.c"
#else
#define LL_RCC_GetUSARTClockFreq(source) ((void)(source), 16000000U)
#include "../../3rdparty/st_stm32l4/src_hal/stm32l4xx_ll_usart.c"
#endif
static uint32_t now_ms;
#define uni_hal_systick_get_ms() (now_ms++)
#define uni_hal_rcc_clk_set(instance, enabled) ((void)(instance), (enabled))
#define uni_hal_rcc_clksrc_set(instance, source) ((void)(instance), (void)(source))
#define uni_hal_gpio_pin_init(pin) ((pin) != nullptr)
#include "uart/uni_hal_uart_stm32l4.c"

#if configAPPLICATION_ALLOCATED_HEAP
alignas(portBYTE_ALIGNMENT) uint8_t ucHeap[configTOTAL_HEAP_SIZE];
#endif

static uni_hal_io_tx_trigger_fn trigger;
static unsigned starts, ends, completions;
static void start(void *io, void *cookie) { (void)io; (void)cookie; ++starts; }
static void end(void *io, void *cookie) { (void)io; (void)cookie; ++ends; }
static bool complete(void *ctx, void *cookie, uni_hal_usart_callback_e type) {
    (void)ctx;
    (void)cookie;
    if (type == UNI_HAL_USART_CALLBACK_TC) ++completions;
    return false;
}

static void delayed_trigger(void *io, void *ctx) {
    // TXE was already enabled. The IRQ can take the newly queued byte before its producer runs
    // the trigger, and the final frame can finish while the producer is still preempted.
    uart.ISR |= USART_ISR_TXE;
    test_irq();
    uart.ISR |= USART_ISR_TXE;
    test_irq();
    uart.ISR |= USART_ISR_TC;
    trigger(io, ctx);
}

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static const uint8_t marker[] = {'A', 'B'};
static volatile int clear_result;
static volatile bool reader_matched;

static bool clear_after_transmit(void *ctx, void *cookie, uni_hal_usart_callback_e type) {
    (void)cookie;
    if (type == UNI_HAL_USART_CALLBACK_TC && xPortIsInsideInterrupt()) {
        clear_result = uni_hal_usart_receive_clear((uni_hal_usart_context_t *)ctx);
    }
    return false;
}

static void pend_uart_irq(void) {
    NVIC_SetPendingIRQ(TEST_IRQn);
    __DSB();
    __ISB();
}

static int clear_from_callback(uni_hal_usart_context_t *ctx) {
    uint8_t const byte = '!';
    clear_result = -1;
    if (uni_hal_usart_transmit_data(ctx, &byte, 1U) == 1U) {
        uart.ISR |= USART_ISR_TXE;
        pend_uart_irq();
        uart.ISR |= USART_ISR_TXE | USART_ISR_TC;
        pend_uart_irq();
    }
    return clear_result;
}

static void waiting_reader(void *io) {
    reader_matched = uni_hal_io_receive_sync(io, marker, sizeof(marker), 1000U);
    vTaskDelete(nullptr);
}

static unsigned check_clear(uni_hal_usart_context_t *ctx) {
    uni_hal_io_context_t *io = ctx->io;
    CHECK(uni_hal_usart_set_callback(ctx, clear_after_transmit, nullptr));
    NVIC_SetPriority(TEST_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS));
    NVIC_EnableIRQ(TEST_IRQn);

    CHECK(xStreamBufferSend(io->buf_rx.handle, "A", 1, 0) == 1U);
    CHECK(!uni_hal_io_receive_sync(io, marker, sizeof(marker), 0));
    CHECK(xStreamBufferSend(io->buf_rx.handle, "X", 1, 0) == 1U);
    CHECK(clear_from_callback(ctx) == 1);
    CHECK(uni_hal_usart_receive_available(ctx) == 0U);
    CHECK(xStreamBufferSend(io->buf_rx.handle, "B", 1, 0) == 1U);
    CHECK(!uni_hal_io_receive_sync(io, marker, sizeof(marker), 0));

    // A higher-priority reader gets as far as blocking on the missing second byte.
    CHECK(xStreamBufferSend(io->buf_rx.handle, "A", 1, 0) == 1U);
    CHECK(!uni_hal_io_receive_sync(io, marker, sizeof(marker), 0));
    CHECK(xTaskCreate(waiting_reader, "reader", 384, io, 2, nullptr) == pdPASS);
    CHECK(clear_from_callback(ctx) == 0 && io->buf_rx.sync_idx == 1U);
    CHECK(xStreamBufferSend(io->buf_rx.handle, "B", 1, 0) == 1U);
    CHECK(reader_matched);
    NVIC_DisableIRQ(TEST_IRQn);
    return 0;
}

static unsigned check(void) {
    UNI_HAL_IO_DEFINITION(io, 8, 8);
    uni_hal_gpio_pin_context_t pin = {};
    uni_hal_usart_context_t ctx = {.instance = TEST_INSTANCE, .io = &io_ctx,
                                   .pin_tx = &pin, .pin_rx = &pin, .baudrate = 115200U};
    // First startup misses the receiver acknowledgement. Retry after the clock recovers.
    uart.ISR = USART_ISR_TEACK;
    CHECK(!uni_hal_usart_init(&ctx));
    CHECK(!ctx.initialized && !io_ctx.buf_rx.handle && !io_ctx.buf_tx.handle);
    bool const disabled_after_timeout = !LL_USART_IsEnabled(&uart);
    uart.ISR = USART_ISR_REACK | USART_ISR_TEACK | USART_ISR_TXE;
    CHECK(uni_hal_usart_init(&ctx));
    CHECK(disabled_after_timeout);
    io_ctx.handlers.tx_start = start;
    io_ctx.handlers.tx_end = end;
    CHECK(uni_hal_usart_set_callback(&ctx, complete, nullptr));

    uint8_t const first = 0x35, second = 0x79;
    CHECK(uni_hal_usart_transmit_data(&ctx, &first, 1) == 1U);
    test_irq();
    CHECK(uart.TDR == first && starts == 1U && completions == 0U);
    trigger = io_ctx.handlers.tx_trigger;
    io_ctx.handlers.tx_trigger = delayed_trigger;
    CHECK(uni_hal_usart_transmit_data(&ctx, &second, 1) == 1U);
    io_ctx.handlers.tx_trigger = trigger;
    test_irq();
    CHECK(uart.TDR == second);
    CHECK(!uni_hal_usart_transmit_busy(&ctx));
    CHECK(starts == 1U && ends == 1U && completions == 1U);
    CHECK(!(uart.CR1 & (USART_CR1_TXEIE | USART_CR1_TCIE)));

    CHECK(uni_hal_usart_irq_enable(&ctx, false));
    CHECK(uni_hal_usart_transmit_data(&ctx, &first, 1) == 1U);
    CHECK(!(uart.CR1 & (USART_CR1_TXEIE | USART_CR1_TCIE)));
    CHECK(uni_hal_usart_irq_enable(&ctx, true));
    test_irq();
    CHECK(uart.TDR == first && starts == 2U && ends == 1U);
    uart.ISR |= USART_ISR_TXE;
    test_irq();
    CHECK((uart.CR1 & (USART_CR1_TXEIE | USART_CR1_TCIE)) == USART_CR1_TCIE);
    CHECK(uni_hal_usart_transmit_busy(&ctx));

    CHECK(uni_hal_usart_irq_enable(&ctx, false));
    uart.ISR |= USART_ISR_TC;
    CHECK(uni_hal_usart_irq_enable(&ctx, true));
    test_irq();
    test_irq();
    CHECK(ends == 2U && completions == 2U && !uni_hal_usart_transmit_busy(&ctx));
    CHECK(io_ctx.stats.tx_transmited == 3U);
    unsigned const clear_error = check_clear(&ctx);
    if (clear_error) return clear_error;
    uni_hal_io_deinit(&io_ctx);
    return 0;
}

[[noreturn]] static void finish(unsigned line) {
    uint32_t result[] = {0x20026U, line};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = result;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

static void run_in_task(void *unused) {
    (void)unused;
    finish(check());
}

int main(void) {
    if (xTaskCreate(run_in_task, "uart", 512, nullptr, 1, nullptr) == pdPASS) {
        vTaskStartScheduler();
    }
    finish(__LINE__);
}

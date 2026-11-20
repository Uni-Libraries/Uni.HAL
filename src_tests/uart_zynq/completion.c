#include "model.h"
#include "xparameters_ps.h"
#include "uart/uni_hal_uart.h"

static bool send_from_callback;
static unsigned starts[2], ends[2], completions[2];
static void start(void *io, void *arg) { (void)io; ++starts[(uintptr_t)arg]; }
static void end(void *io, void *arg) { (void)io; ++ends[(uintptr_t)arg]; }
static bool done(void *ctx, void *arg, uni_hal_usart_callback_e type) {
    if (type == UNI_HAL_USART_CALLBACK_TC && uart_model_in_isr()) ++completions[(uintptr_t)arg];
    if (send_from_callback && type == UNI_HAL_USART_CALLBACK_TC) {
        send_from_callback = false;
        uint8_t next = 0x7EU;
        uni_hal_usart_transmit_data((uni_hal_usart_context_t *)ctx, &next, 1U);
    }
    return true;
}
#define CHECK(test) do { if (!(test)) return __LINE__; } while (false)
int main(void) {
    static uint8_t rx[2][16], tx[2][16];
    uni_hal_io_context_t io[2] = {};
    uni_hal_usart_context_t uart[2] = {};
    for (unsigned p = 0; p < 2U; ++p) {
        io[p].buf_tx.array = tx[p];
        io[p].buf_tx.size = sizeof tx[p];
        io[p].buf_rx.array = rx[p];
        io[p].buf_rx.size = sizeof rx[p];
        uart[p].io = &io[p];
        uart[p].instance = p ? UNI_HAL_CORE_PERIPH_UART_2 : UNI_HAL_CORE_PERIPH_UART_1;
        if (p == 0U) {
            uart_model_ppi_available = false;
            CHECK(!uni_hal_usart_init(&uart[p]));
            uart_model_ppi_available = true;
            uart_model_timer_control = 7U;
            CHECK(!uni_hal_usart_init(&uart[p]) && uart_model_timer_control == 7U);
            uart_model_timer_control = 0U;
            CHECK(!uni_hal_usart_init(&uart[p]) && uart_model_timer_control == 0U);
            uart_model_timer_control = 0x201U; // Enabled timer with a non-default prescaler.
            uart_model_fail_irq = XPS_GLOBAL_TMR_INT_ID;
            CHECK(!uni_hal_usart_init(&uart[p]) && !uart_model_irq_connected(XPS_UART0_INT_ID));
            uart_model_fail_irq = XPS_UART0_INT_ID;
            CHECK(!uni_hal_usart_init(&uart[p]) && !uart_model_irq_connected(XPS_GLOBAL_TMR_INT_ID));
            uart_model_fail_irq = 0U;
        }
        CHECK(uni_hal_usart_init(&uart[p]));
        io[p].handlers.tx_start = start;
        io[p].handlers.tx_start_ctx = (void *)(uintptr_t)p;
        io[p].handlers.tx_end = end;
        io[p].handlers.tx_end_ctx = (void *)(uintptr_t)p;
        CHECK(uni_hal_usart_set_callback(&uart[p], done, (void *)(uintptr_t)p));
    }
    uint8_t a = 0x31U, b = 0x32U;
    CHECK(uni_hal_usart_transmit_data(&uart[0], &a, 1U) == 1U && starts[0] == 1U);
    uart_model_drain(0, true);
    uart_model_irq(0);
    CHECK(uni_hal_usart_transmit_busy(&uart[0]) && ends[0] == 0U && completions[0] == 0U);
    CHECK(uart_model_alarm_armed());
    uart_model_alarm(); // CTS or a slow transmitter can delay wire idle beyond one check.
    CHECK(uart_model_alarm_armed() && ends[0] == 0U);
    CHECK(uni_hal_usart_transmit_data(&uart[0], &b, 1U) == 1U);
    CHECK(uart_model_tx_count(0) == 2U && uart_model_tx_byte(0, 1) == b && starts[0] == 1U);
    uart_model_drain(0, true);
    uart_model_irq(0);
    CHECK(uni_hal_usart_transmit_data(&uart[1], &a, 1U) == 1U);
    uart_model_drain(1, true);
    uart_model_irq(1);
    uart_model_finish(0);
    uart_model_alarm();
    CHECK(!uni_hal_usart_transmit_busy(&uart[0]) && ends[0] == 1U && completions[0] == 1U);
    CHECK(uni_hal_usart_transmit_busy(&uart[1]) && ends[1] == 0U && uart_model_alarm_armed());
    uart_model_finish(1);
    uart_model_alarm();
    CHECK(!uni_hal_usart_transmit_busy(&uart[1]) && completions[1] == 1U && !uart_model_alarm_armed());
    CHECK(uni_hal_usart_transmit_data(&uart[0], &a, 1U) == 1U);
    uart_model_drain(0, false); // Delayed UART interrupt arrives after shift register emptied.
    uart_model_irq(0);
    CHECK(ends[0] == 2U && completions[0] == 2U && !uart_model_alarm_armed());
    CHECK(uni_hal_usart_transmit_data(&uart[0], &a, 1U) == 1U);
    uart_model_drain(0, true);
    uart_model_irq(0);
    send_from_callback = true;
    uart_model_finish(0);
    uart_model_alarm(); // TC callback immediately starts a new burst from the timer IRQ.
    CHECK(uni_hal_usart_transmit_busy(&uart[0]) && uart_model_tx_byte(0, 4U) == 0x7EU);
    uart_model_drain(0, false);
    uart_model_irq(0);
    CHECK(!uni_hal_usart_transmit_busy(&uart[0]) && starts[0] == 4U && completions[0] == 4U);
    CHECK(uart_model_timer_control == 0x201U);
    CHECK(uart_model_bad_task_critical == 0U && uart_model_yields >= 3U);
    return 0;
}

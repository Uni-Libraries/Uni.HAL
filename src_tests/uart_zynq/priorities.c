#include "model.h"
#include "xparameters_ps.h"
#include "zynq_core.h"
#include "uart/uni_hal_uart.h"

#define CHECK(test) do { if (!(test)) return __LINE__; } while (false)

int main(void) {
    UNI_HAL_IO_DEFINITION(first, 8, 8);
    UNI_HAL_IO_DEFINITION(second, 8, 8);
    uni_hal_usart_context_t uart = {.instance = UNI_HAL_CORE_PERIPH_UART_1, .io = &first_ctx};
    uint32_t const minimum = configMAX_API_CALL_INTERRUPT_PRIORITY;
    uint32_t const invalid[] = {minimum - 1U, 31U, 32U};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        uart.isr_priority = invalid[i];
        CHECK(!uni_hal_usart_init(&uart));
        CHECK(!uart_model_irq_connected(XPS_GLOBAL_TMR_INT_ID) && !uart_model_irq_connected(XPS_UART0_INT_ID));
        CHECK(first_ctx.buf_rx.handle == NULL && first_ctx.buf_tx.handle == NULL);
    }
    uart.isr_priority = minimum;
    CHECK(uni_hal_usart_init(&uart));
    CHECK(uart_model_irq_priority(XPS_UART0_INT_ID) == minimum);

    unsigned const default_priority = minimum > 20U ? minimum : 20U;
    CHECK(uart_model_irq_priority(XPS_GLOBAL_TMR_INT_ID) == default_priority);
    uni_hal_usart_context_t other = {.instance = UNI_HAL_CORE_PERIPH_UART_2, .io = &second_ctx};
    CHECK(uni_hal_usart_init(&other));
    CHECK(uart_model_irq_priority(XPS_UART1_INT_ID) == default_priority);
    return 0;
}

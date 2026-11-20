#include <stddef.h>

#include "io/uni_hal_io_tunnel.h"
#include "uart/uni_hal_uart.h"
#include "model.h"

void tunnel_model_wake_on(StreamBufferHandle_t send, StreamBufferHandle_t pop);

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

int main(void) {
    UNI_HAL_IO_DEFINITION(first, 8, 8);
    UNI_HAL_IO_DEFINITION(second, 8, 8);
    uni_hal_usart_context_t from = {.instance = UNI_HAL_CORE_PERIPH_UART_1, .io = &first_ctx, .baudrate = 115200};
    uni_hal_usart_context_t to = {.instance = UNI_HAL_CORE_PERIPH_UART_2, .io = &second_ctx, .baudrate = 115200};
    uni_hal_io_tunnel_context_t tunnel = {};
    CHECK(uni_hal_usart_init(&from) && uni_hal_usart_init(&to));
    CHECK(uni_hal_io_tunnel_init(&tunnel, &first_ctx, &second_ctx, false));

    // The first byte must start a previously idle UART from the other UART's receive interrupt.
    tunnel_model_wake_on(second_ctx.buf_tx.handle, NULL);
    unsigned yields = uart_model_yields;
    uart_model_rx(0, 0x41);
    uart_model_irq(0);
    CHECK(uart_model_bad_task_critical == 0);
    CHECK(uart_model_tx_count(1) == 1 && uart_model_tx_byte(1, 0) == 0x41);
    CHECK(uart_model_yields > yields && uni_hal_io_receive_available(&first_ctx) == 0);

    // A TX pop can also wake a writer. Transparent forwarding preserves the local input.
    CHECK(uni_hal_io_tunnel_init(&tunnel, &first_ctx, &second_ctx, true));
    tunnel_model_wake_on(NULL, first_ctx.buf_tx.handle);
    yields = uart_model_yields;
    uart_model_rx(1, 0x5A);
    uart_model_irq(1);
    CHECK(uart_model_bad_task_critical == 0);
    CHECK(uart_model_tx_count(0) == 1 && uart_model_tx_byte(0, 0) == 0x5A);
    CHECK(uart_model_yields > yields);
    uint8_t received = 0;
    CHECK(uni_hal_io_receive_data(&second_ctx, &received, 1, 0) == 1 && received == 0x5A);

    // A full destination leaves the unforwarded byte on the source in opaque mode.
    CHECK(uni_hal_io_tunnel_init(&tunnel, &first_ctx, &second_ctx, false));
    uint8_t filler[8] = {};
    size_t const free = xStreamBufferSpacesAvailable(second_ctx.buf_tx.handle);
    CHECK(free && free <= sizeof filler && xStreamBufferSend(second_ctx.buf_tx.handle, filler, free, 0) == free);
    uart_model_rx(0, 0x7B);
    uart_model_irq(0);
    CHECK(uart_model_tx_count(1) == 1);
    CHECK(uni_hal_io_receive_data(&first_ctx, &received, 1, 0) == 1 && received == 0x7B);
    uni_hal_io_deinit(&first_ctx);
    uni_hal_io_deinit(&second_ctx);
    return 0;
}

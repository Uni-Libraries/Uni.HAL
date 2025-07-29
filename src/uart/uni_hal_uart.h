#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stdint.h>

// uni_hal
#include "uni_common.h"
#include "gpio/uni_hal_gpio.h"
#include "io/uni_hal_io.h"
#include "rcc/uni_hal_rcc.h"

//
// Typedefs
//

/**
 * USART interface context
 */
typedef struct {
    /**
     * UART instance
     */
    uni_hal_core_periph_e instance;

    /**
     * Clock source
     */
    uni_hal_rcc_clksrc_e clksrc;

    /**
     * UART RX pin
     */
    uni_hal_gpio_pin_context_t* pin_rx;

    /**
     * UART TX pin
     */
    uni_hal_gpio_pin_context_t* pin_tx;

    /**
     * IO context
     */
    uni_hal_io_context_t *io;

    /*
     * Was USART initialized
     */
    bool initialized;

    /**
     * Was USART in transmission state
     */
    bool in_transmission;

} uni_hal_usart_context_t;


//
// Functions
//

/**
 * Initializes USART interface
 * @param ctx pointer to interface context
 * @return true on success
 */
bool uni_hal_usart_init(uni_hal_usart_context_t *ctx);

/**
 * Checks that UART is inited
 * @param ctx UART context
 * @return true in case inited UART
 */
bool uni_hal_uart_is_inited(const uni_hal_usart_context_t *ctx);

/**
 * Gets current USART baudrate
 * @param ctx pointer to interface context
 * @return baudrate in b/s
 */
uint32_t uni_hal_usart_baudrate_get(uni_hal_usart_context_t *ctx);

/**
 * Sets given baudrate to the USART interface
 * @param ctx pointer to interface context
 * @param baudrate baudrate in b/s
 * @return true on success
 */
bool uni_hal_usart_baudrate_set(uni_hal_usart_context_t *ctx, uint32_t baudrate);

/**
 * Enables or disables information receiving
 * @param ctx pointer to interface context
 * @param value true to enable, false to disable
 * @return true on success
 */
bool uni_hal_usart_receive_enable(uni_hal_usart_context_t *ctx, bool value);


/**
 * Enables or disables information transmit
 * @param ctx pointer to interface context
 * @param value true to enable, false to disable
 * @return true on success
 */
bool uni_hal_usart_transmit_enable(uni_hal_usart_context_t *ctx, bool value);

/**
 * Receive IO context context
 * @param ctx pointer to the UART context
 * @return true on sucess
 */
uni_hal_io_context_t* uni_hal_uart_io_get(uni_hal_usart_context_t *ctx);



//
// Defines
//

#define uni_hal_usart_receive_available(ctx) uni_hal_io_receive_available((ctx)->io)
#define uni_hal_usart_receive_clear(ctx) uni_hal_io_receive_clear((ctx)->io)
#define uni_hal_usart_receive_data(ctx, data, data_len, timeout) uni_hal_io_receive_data((ctx)->io, data, data_len, timeout)
#define uni_hal_usart_transmit_data(ctx, data, data_len) uni_hal_io_transmit_data((ctx)->io, data, data_len)

#if defined(__cplusplus)
}
#endif

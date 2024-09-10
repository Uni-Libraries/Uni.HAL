#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

#include <stdbool.h>

#include "uni_hal_io.h"


//
// Typedefs
//

/**
 * IO tunnel context
 */
typedef struct {
    /**
     * The first IO interface
     */
    uni_hal_io_context_t *io_first;

    /**
     * The second IO interface
     */
    uni_hal_io_context_t *io_second;

    /**
     * Use transparent mode
     */
    bool transparent;
} uni_hal_io_tunnel_context_t;


typedef enum {
    UNI_HAL_IO_TUNNEL_FAIL = 0,
    UNI_HAL_IO_TUNNEL_OK = 1,
    UNI_HAL_IO_TUNNEL_TRANSPARENT = 2
} uni_hal_io_tunnel_transmit_result_t;

//
// Functions
//

/**
 * Enables or disables IO tunnel
 * @param ctx IO tunnel context
 * @param io_first pointer to the first IO context
 * @param io_second pointer to the second IO context
 * @param transparent set to true to not drop data from the IO RX buffer
 * @return true on success
 * @note pass NULL to the io_first and io_second do stop the tunnel
 */
bool uni_hal_io_tunnel_init(uni_hal_io_tunnel_context_t *ctx, uni_hal_io_context_t *io_first,
                           uni_hal_io_context_t *io_second, bool transparent);


/**
 * Check that tunnel is inited
 * @param ctx IO tunnel context
 * @return true in case of inited tunnel
 */
bool bsuat_io_tunnel_is_inited(const uni_hal_io_tunnel_context_t *ctx);


/**
 * Transmit data via tunnel
 * @param ctx pointer to the IO tunnel context
 * @param io_from originating IO context
 * @param data data to transmit
 * @param data_len length of data to transmit
 * @return one of ::uni_hal_io_tunnel_transmit_result_t values
 */
uni_hal_io_tunnel_transmit_result_t uni_hal_io_tunnel_transmit(uni_hal_io_tunnel_context_t *ctx,
                                                             uni_hal_io_context_t *io_from, uint8_t *data,
                                                             size_t data_len);

#if defined(__cplusplus)
}
#endif

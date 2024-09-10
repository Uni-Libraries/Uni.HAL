//
// Includes
//

#include "uni_hal_spi.h"



//
// Functions
//

bool uni_hal_spi_init(uni_hal_spi_context_t *ctx){
    (void)ctx;
    return false;
}


bool uni_hal_spi_is_inited(const uni_hal_spi_context_t* ctx){
    (void)ctx;
    return false;
}

bool uni_hal_spi_is_busy(const uni_hal_spi_context_t* ctx) {
    (void)ctx;
    return false;
}


bool uni_hal_spi_set_prescaler(const uni_hal_spi_context_t* ctx, uni_hal_spi_prescaler_e  prescaler) {
    (void)ctx;
    (void)prescaler;
    return false;
}


bool uni_hal_spi_receive(uni_hal_spi_context_t *ctx, uint8_t *data, uint32_t len){
    (void)ctx;
    (void)data;
    (void)len;
    return false;
}


bool uni_hal_spi_transmit(uni_hal_spi_context_t *ctx, const uint8_t *data, uint32_t len){
    (void)ctx;
    (void)data;
    (void)len;
    return false;
}

bool uni_hal_spi_transmit_async(uni_hal_spi_context_t *ctx, const uint8_t *data, uint32_t len) {
    (void)ctx;
    (void)data;
    (void)len;
    return false;
}


bool uni_hal_spi_transmitreceive(uni_hal_spi_context_t *ctx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t len){
    (void)ctx;
    (void)tx_data;
    (void)rx_data;
    (void)len;
    return false;
}

//
// Includes
//

// stdlib
#include <stddef.h>

// ST
#include <stm32l4xx_ll_spi.h>

// Uni.HAL
#include "dwt/uni_hal_dwt.h"
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"
#include "spi/uni_hal_spi.h"


//
// Private
//

static SPI_TypeDef *_uni_hal_spi_handle_get(uni_hal_core_periph_e instance) {
    SPI_TypeDef *result = NULL;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_SPI_1:
        result = SPI1;
        break;
    case UNI_HAL_CORE_PERIPH_SPI_2:
        result = SPI2;
        break;
    case UNI_HAL_CORE_PERIPH_SPI_3:
        result = SPI3;
        break;
    default:
        break;
    }
    return result;
}

bool _uni_hal_spi_clear_flags(uni_hal_core_periph_e instance) {
    bool result = false;

    SPI_TypeDef *handle = _uni_hal_spi_handle_get(instance);
    if (handle != NULL) {
        LL_SPI_ClearFlag_OVR(handle);
        LL_SPI_ClearFlag_FRE(handle);
        LL_SPI_ClearFlag_CRCERR(handle);
        LL_SPI_ClearFlag_MODF(handle);
        result = true;
    }

    return result;
}

/**
 * Convert SPI polarity to vendor speicific
 * @param polarity polarity enum value
 * @return converted value
 */
uint32_t _uni_hal_spi_phase(uni_hal_spi_cpha_e phase){
    uint32_t result = 0;
    switch(phase){
    case UNI_HAL_SPI_CPHA_0:
        result = LL_SPI_PHASE_1EDGE;
        break;
    case UNI_HAL_SPI_CPHA_1:
        result = LL_SPI_PHASE_2EDGE;
        break;
    default:
        break;
    }
    return result;
}


/**
 * Convert SPI polarity to vendor speicific
 * @param polarity polarity enum value
 * @return converted value
 */
uint32_t _uni_hal_spi_polarity(uni_hal_spi_cpol_e polarity){
    uint32_t result = 0;
    switch(polarity){
    case UNI_HAL_SPI_CPOL_0:
        result = LL_SPI_POLARITY_LOW;
        break;
    case UNI_HAL_SPI_CPOL_1:
        result = LL_SPI_POLARITY_HIGH;
        break;
    default:
        break;
    }
    return result;
}



//
// Public
//

bool uni_hal_spi_init(uni_hal_spi_context_t *ctx) {
    bool result = false;

    if (ctx != NULL && !uni_hal_spi_is_inited(ctx)) {
        SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
        if (instance != NULL) {
            // clock
            result = uni_hal_rcc_clk_set(ctx->config.instance, true);

            // gpio
            result = uni_hal_gpio_pin_init(ctx->config.pin_sck) && result;
            result = uni_hal_gpio_pin_init(ctx->config.pin_miso) && result;
            result = uni_hal_gpio_pin_init(ctx->config.pin_mosi) && result;

            // spi
            LL_SPI_InitTypeDef SPI_InitStruct = {0};
            SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
            SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
            SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
            SPI_InitStruct.ClockPolarity = _uni_hal_spi_polarity(ctx->config.polarity);
            SPI_InitStruct.ClockPhase = _uni_hal_spi_phase(ctx->config.phase);
            SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
            SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV4;
            SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
            SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
            SPI_InitStruct.CRCPoly = 7;
            result = (LL_SPI_Init(instance, &SPI_InitStruct) == SUCCESS) && result;
            if (result) {
                LL_SPI_SetStandard(instance, LL_SPI_PROTOCOL_MOTOROLA);
                LL_SPI_DisableNSSPulseMgt(instance);
            }
            ctx->status.inited = result;
        }
    }

    return result;
}


bool uni_hal_spi_is_inited(const uni_hal_spi_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = ctx->status.inited;
    }
    return result;
}


bool uni_hal_spi_receive(uni_hal_spi_context_t *ctx, uint8_t *data, uint32_t len) {
    return uni_hal_spi_transmitreceive(ctx, NULL, data, len);
}


bool uni_hal_spi_transmit(uni_hal_spi_context_t *ctx, const uint8_t *data, uint32_t len) {
    return uni_hal_spi_transmitreceive(ctx, data, NULL, len);
}

bool uni_hal_spi_transmitreceive(uni_hal_spi_context_t *ctx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t len) {
    bool result = false;
    uint8_t data_rx = 0U;

    if (uni_hal_spi_is_inited(ctx)) {
        SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
        if (instance != NULL) {
            _uni_hal_spi_clear_flags(ctx->config.instance);

            LL_SPI_SetRxFIFOThreshold(instance, LL_SPI_RX_FIFO_TH_QUARTER);
            LL_SPI_Enable(instance);

            for (size_t idx = 0; idx < len; idx++) {
                while (!LL_SPI_IsActiveFlag_TXE(instance)) {
                }
                LL_SPI_TransmitData8(instance, tx_data ? tx_data[idx] : 0U);

                while (!LL_SPI_IsActiveFlag_RXNE(instance)) {
                }
                data_rx = LL_SPI_ReceiveData8(instance);
                if (rx_data) {
                    rx_data[idx] = data_rx;
                }
            }

            while (LL_SPI_IsActiveFlag_BSY(instance)) {
            }

            LL_SPI_Disable(instance);
            result = true;
        }
    }

    return result;
}

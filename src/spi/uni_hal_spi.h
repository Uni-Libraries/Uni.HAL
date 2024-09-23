#pragma once

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stdint.h>

// uni_hal
#include "core/uni_hal_core.h"
#include "dma/uni_hal_dma.h"
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"


//
// Typedefs
//

/*
 * SPI prescaler
 */
typedef enum {
    UNI_HAL_SPI_PRESCALER_2,
    UNI_HAL_SPI_PRESCALER_4,
    UNI_HAL_SPI_PRESCALER_8,
    UNI_HAL_SPI_PRESCALER_16,
    UNI_HAL_SPI_PRESCALER_32,
    UNI_HAL_SPI_PRESCALER_64,
    UNI_HAL_SPI_PRESCALER_128,
    UNI_HAL_SPI_PRESCALER_256,
} uni_hal_spi_prescaler_e;

/**
 * SPI mode
 */
typedef enum {
    UNI_HAL_SPI_MODE_MASTER,
    UNI_HAL_SPI_MODE_SLAVE,
} uni_hal_spi_mode_e;

/**
 * SPI polarity
 */
typedef enum {
    UNI_HAL_SPI_CPOL_0,
    UNI_HAL_SPI_CPOL_1,
} uni_hal_spi_cpol_e;

/**
 * SPI phase
 */
typedef enum {
    UNI_HAL_SPI_CPHA_0,
    UNI_HAL_SPI_CPHA_1
} uni_hal_spi_cpha_e;

/**
 * SPI config
 */
typedef struct {
    /**
     * SPI instance
     */
    uni_hal_core_periph_e instance;

    /**
     * SPI mode
     */
    uni_hal_spi_mode_e mode;

    /**
     * SPI clock source
     */
    uni_hal_rcc_clksrc_e clock_source;

    /**
     * DMA TX context
     */
    uni_hal_dma_context_t* dma_tx;

    /**
     * DMA RX context
     */
    uni_hal_dma_context_t* dma_rx;

    /**
     * MISO pin
     */
    uni_hal_gpio_pin_context_t *pin_miso;

    /**
     * MOSI pin
     */
    uni_hal_gpio_pin_context_t *pin_mosi;

    /**
     * SCK pin
     */
    uni_hal_gpio_pin_context_t *pin_sck;

    /**
     * SSS pin
     */
    uni_hal_gpio_pin_context_t *pin_nss;

    /**
     * SPI polarity
     */
    uni_hal_spi_cpol_e polarity;

    /**
     * SPI phase
     */
    uni_hal_spi_cpha_e phase;

    /**
     * SPI prescaler
     */
    uni_hal_spi_prescaler_e prescaler;

    /**
     * Use hardware NSS control
     */
    bool nss_hard;
} uni_hal_spi_config_t;


/**
 * SPI status
 */
typedef struct {
    /**
     * Inited
     */
    bool inited;

    /**
     * SPI transmission is in progress
     */
    bool in_process;
} uni_hal_spi_status_t;


/**
 * SPI context
 */
typedef struct {
    /**
     * SPI config
     */
    uni_hal_spi_config_t config;

    /**
     * SPI status
     */
     uni_hal_spi_status_t status;
} uni_hal_spi_context_t;


//
// Functions
//

/**
 * Initialize SPI
 * @param ctx SPI context
 * @return true on success
 */
bool uni_hal_spi_init(uni_hal_spi_context_t *ctx);


/**
 * Chech that SPI was initialized
 * @param ctx SPI context
 * @return true on success
 */
bool uni_hal_spi_is_inited(const uni_hal_spi_context_t* ctx);

/**
 * Check that SPI is busy
 * @param ctx SPI context
 * @return true in case it is busy
 */
bool uni_hal_spi_is_busy(const uni_hal_spi_context_t* ctx);


bool uni_hal_spi_set_prescaler(uni_hal_spi_context_t* ctx, uni_hal_spi_prescaler_e  prescaler);

/**
 * Receive data from SPI bus
 * @param ctx SPI context
 * @param data receive buffer pointer
 * @param len number of bytes to receive
 * @return true on success
 */
bool uni_hal_spi_receive(uni_hal_spi_context_t *ctx, uint8_t *data, uint32_t len);


/**
 * Transmit data to the SPI bus
 * @param ctx SPI context
 * @param data transmit buffer pointer
 * @param len number of bytes to transmit
 * @return true on success
 */
bool uni_hal_spi_transmit(uni_hal_spi_context_t *ctx, const uint8_t *data, uint32_t len);


bool uni_hal_spi_transceive_async(uni_hal_spi_context_t *ctx, const uint8_t *data_rx, const uint8_t *data_tx, uint32_t len);



/**
 * Receive & Transmit data using SPI bus
 * @param ctx SPI context
 * @param tx_data transmit buffer
 * @param rx_data receive buffer
 * @param len number of bytes to transmit
 * @return true on success
 */
bool uni_hal_spi_transmitreceive(uni_hal_spi_context_t *ctx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t len);

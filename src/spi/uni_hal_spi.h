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
#include <stddef.h>

// uni_hal
#include "uni_hal_target.h"
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
 * SPI CRC
 */
typedef enum {
    UNI_HAL_SPI_CRC_DISABLE,
    UNI_HAL_SPI_CRC_16BIT,
} uni_hal_spi_crc_type_e;

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
     * SPI clock source. On STM32H743/H753, SPI1/2/3 share one selection and SPI4/5
     * share another. Initialization fails if a sibling is already using a different source.
     */
    uni_hal_rcc_clksrc_e clock_source;

    /**
     * DMA TX context
     */
    uni_hal_dma_context_t *dma_tx;

    /**
     * DMA RX context
     */
    uni_hal_dma_context_t *dma_rx;

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

    /**
     * CRC type
     */
    uni_hal_spi_crc_type_e crc_type;

    /**
     * CRC polynomial
     */
    uint32_t crc_polynomial;

    /**
     * CRC init pattern
     */
    uint32_t crc_init;
} uni_hal_spi_config_t;


/**
 * SPI transfer errors, combined as bits in uni_hal_spi_status_t::last_errors
 */
typedef enum {
    UNI_HAL_SPI_ERROR_NONE       = 0,
    UNI_HAL_SPI_ERROR_CRC        = 1 << 0,
    UNI_HAL_SPI_ERROR_OVERRUN    = 1 << 1,
    UNI_HAL_SPI_ERROR_UNDERRUN   = 1 << 2,
    UNI_HAL_SPI_ERROR_MODE_FAULT = 1 << 3,
    UNI_HAL_SPI_ERROR_DMA        = 1 << 4,
    UNI_HAL_SPI_ERROR_FRAME      = 1 << 5,
    UNI_HAL_SPI_ERROR_SUSPENDED  = 1 << 6,
} uni_hal_spi_error_e;


/**
 * SPI callback
 */
typedef bool (*uni_hal_spi_callback_t)(void *cookie);


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
    volatile bool in_process;

    uni_hal_spi_callback_t callback;

    void *callback_cookie;
    uint8_t *last_rx_data;
    size_t last_len;

    /**
     * Errors of the last asynchronous transfer, bits of uni_hal_spi_error_e
     */
    uint32_t last_errors;
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
 * Initialize SPI. On STM32H743/H753 keep the context and its configuration alive while
 * registered. Another context can take over the same peripheral once it is idle.
 * @param ctx SPI context
 * @return true on success
 */
bool uni_hal_spi_init(uni_hal_spi_context_t *ctx);


/**
 * Chech that SPI was initialized
 * @param ctx SPI context
 * @return true on success
 */
bool uni_hal_spi_is_inited(const uni_hal_spi_context_t *ctx);

#if defined(UNI_HAL_TARGET_STM32H7) || defined(UNI_HAL_TARGET_MCU_PC)
/**
 * Check that SPI is busy
 * @param ctx SPI context
 * @return true in case it is busy
 */
bool uni_hal_spi_is_busy(const uni_hal_spi_context_t *ctx);


bool uni_hal_spi_set_prescaler(uni_hal_spi_context_t *ctx, uni_hal_spi_prescaler_e prescaler);
#endif

/**
 * Gets current SPI bitrate (SCK frequency) in bits per second.
 *
 * @note For master mode, this is calculated as: SPI kernel clock / baudrate prescaler.
 *       For slave mode, the bitrate is driven externally, so this function returns 0.
 *
 * @param ctx SPI context
 * @return bitrate in b/s, or 0 if not available
 */
uint32_t uni_hal_spi_bitrate_get(uni_hal_spi_context_t *ctx);

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


#if defined(UNI_HAL_TARGET_STM32H7) || defined(UNI_HAL_TARGET_MCU_PC)
/**
 * Start a DMA transfer on STM32H743/H753 SPI1..5. Length is 1..65535 bytes. Supply an RX
 * buffer, a TX buffer, or both. Each direction needs its own configured DMA stream.
 * Buffers must be accessible to DMA1/2. Full duplex accepts disjoint ranges or one
 * shared in-place buffer. After a successful start, keep TX unchanged and leave RX
 * untouched until the completion callback or until is_busy() returns false.
 * A rejected call leaves the active transfer intact. Cacheable RX buffers must own
 * all cache lines touched by the transfer. The callback may start the next transfer.
 */
bool uni_hal_spi_transceive_async(uni_hal_spi_context_t *ctx, uint8_t *data_rx, const uint8_t *data_tx,
                                  uint32_t len);
#endif


/**
 * Stop an asynchronous transfer that is still running, for instance a slave transfer whose
 * master never sent its clocks. The completion callback is suppressed on cancellation.
 * On STM32H743/H753, false with is_busy() still true can mean a pending cancellation:
 * keep the buffers until is_busy() becomes false. A master first requests suspension
 * at a frame boundary. A second abort can truncate the frame if the bus is stalled.
 * A DMA beat already in progress must finish before buffer ownership returns.
 * @param ctx SPI context
 * @return true when a transfer was in progress and has been stopped
 */
bool uni_hal_spi_abort(uni_hal_spi_context_t *ctx);


bool uni_hal_spi_set_callback(uni_hal_spi_context_t *ctx, uni_hal_spi_callback_t callback, void *cookie);


/**
 * Get the errors of the last asynchronous transfer.
 * The value is valid from the completion callback on, until the next transfer is started.
 * @param ctx SPI context
 * @return bits of uni_hal_spi_error_e, UNI_HAL_SPI_ERROR_NONE when the transfer was clean
 */
uint32_t uni_hal_spi_get_last_errors(const uni_hal_spi_context_t *ctx);


/**
 * Receive & Transmit data using SPI bus
 * @param ctx SPI context
 * @param tx_data transmit buffer
 * @param rx_data receive buffer
 * @param len number of bytes to transmit
 * @return true on success
 */
bool uni_hal_spi_transmitreceive(uni_hal_spi_context_t *ctx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t len);

#if defined(__cplusplus)
}
#endif

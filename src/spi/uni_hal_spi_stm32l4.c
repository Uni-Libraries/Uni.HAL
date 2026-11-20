//
// Includes
//

// stdlib
#include <stddef.h>

// ST
#include <stm32l4xx_ll_rcc.h>
#include <stm32l4xx_ll_spi.h>

// Uni.HAL
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"
#include "spi/uni_hal_spi.h"
#include "systick/uni_hal_systick.h"

/**
 * A blocking master transfer is abandoned when the bus makes no progress for this long.
 * A slave waits for its master for as long as it takes.
 */
#define UNI_HAL_SPI_STALL_TIMEOUT_MS (100U)


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

static const uint32_t prescalers[] = {
    [UNI_HAL_SPI_PRESCALER_2]   = LL_SPI_BAUDRATEPRESCALER_DIV2,
    [UNI_HAL_SPI_PRESCALER_4]   = LL_SPI_BAUDRATEPRESCALER_DIV4,
    [UNI_HAL_SPI_PRESCALER_8]   = LL_SPI_BAUDRATEPRESCALER_DIV8,
    [UNI_HAL_SPI_PRESCALER_16]  = LL_SPI_BAUDRATEPRESCALER_DIV16,
    [UNI_HAL_SPI_PRESCALER_32]  = LL_SPI_BAUDRATEPRESCALER_DIV32,
    [UNI_HAL_SPI_PRESCALER_64]  = LL_SPI_BAUDRATEPRESCALER_DIV64,
    [UNI_HAL_SPI_PRESCALER_128] = LL_SPI_BAUDRATEPRESCALER_DIV128,
    [UNI_HAL_SPI_PRESCALER_256] = LL_SPI_BAUDRATEPRESCALER_DIV256,
};

static bool valid_config(const uni_hal_spi_config_t *config) {
    const uni_hal_rcc_clksrc_e bus_clock = config->instance == UNI_HAL_CORE_PERIPH_SPI_1
        ? UNI_HAL_RCC_CLKSRC_PCLK2 : UNI_HAL_RCC_CLKSRC_PCLK1;
    return (config->clock_source == UNI_HAL_RCC_CLKSRC_UNKNOWN || config->clock_source == bus_clock) &&
           (unsigned)config->mode <= UNI_HAL_SPI_MODE_SLAVE &&
           (unsigned)config->prescaler < sizeof(prescalers) / sizeof(prescalers[0]) &&
           (unsigned)config->polarity <= UNI_HAL_SPI_CPOL_1 && (unsigned)config->phase <= UNI_HAL_SPI_CPHA_1 &&
           config->pin_sck && config->pin_miso && config->pin_mosi && (!config->nss_hard || config->pin_nss) &&
           config->crc_type == UNI_HAL_SPI_CRC_DISABLE && !config->dma_rx && !config->dma_tx;
}

static uint32_t _uni_hal_spi_clk_get(uni_hal_core_periph_e instance) {
    LL_RCC_ClocksTypeDef clocks;
    LL_RCC_GetSystemClocksFreq(&clocks);

    switch (instance) {
        case UNI_HAL_CORE_PERIPH_SPI_1:
            return clocks.PCLK2_Frequency;
        case UNI_HAL_CORE_PERIPH_SPI_2:
        case UNI_HAL_CORE_PERIPH_SPI_3:
            return clocks.PCLK1_Frequency;
        default:
            return 0U;
    }
}

bool uni_hal_spi_init(uni_hal_spi_context_t *ctx) {
    if (!ctx || uni_hal_spi_is_inited(ctx)) {
        return false;
    }
    const uni_hal_spi_config_t *config = &ctx->config;
    SPI_TypeDef *instance = _uni_hal_spi_handle_get(config->instance);
    if (!instance || !valid_config(config)) {
        return false;
    }

    if (!uni_hal_rcc_clk_set(config->instance, true) ||
        !uni_hal_gpio_pin_init(config->pin_sck) || !uni_hal_gpio_pin_init(config->pin_miso) ||
        !uni_hal_gpio_pin_init(config->pin_mosi) || (config->pin_nss && !uni_hal_gpio_pin_init(config->pin_nss))) {
        return false;
    }

    const bool master = config->mode == UNI_HAL_SPI_MODE_MASTER;
    LL_SPI_InitTypeDef init = {
        .TransferDirection = LL_SPI_FULL_DUPLEX,
        .Mode = master ? LL_SPI_MODE_MASTER : LL_SPI_MODE_SLAVE,
        .DataWidth = LL_SPI_DATAWIDTH_8BIT,
        .ClockPolarity = config->polarity == UNI_HAL_SPI_CPOL_0 ? LL_SPI_POLARITY_LOW : LL_SPI_POLARITY_HIGH,
        .ClockPhase = config->phase == UNI_HAL_SPI_CPHA_0 ? LL_SPI_PHASE_1EDGE : LL_SPI_PHASE_2EDGE,
        .NSS = LL_SPI_NSS_SOFT,
        .BaudRate = prescalers[config->prescaler],
        .BitOrder = LL_SPI_MSB_FIRST,
        .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    };
    if (config->nss_hard) {
        init.NSS = master ? LL_SPI_NSS_HARD_OUTPUT : LL_SPI_NSS_HARD_INPUT;
    }
    if (LL_SPI_Init(instance, &init) != SUCCESS) {
        return false;
    }
    LL_SPI_SetStandard(instance, LL_SPI_PROTOCOL_MOTOROLA);
    LL_SPI_DisableNSSPulseMgt(instance);
    ctx->status.inited = true;
    return true;
}


bool uni_hal_spi_is_inited(const uni_hal_spi_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        result = ctx->status.inited;
    }
    return result;
}


uint32_t uni_hal_spi_bitrate_get(uni_hal_spi_context_t *ctx) {
    uint32_t result = 0U;

    if (uni_hal_spi_is_inited(ctx) && (ctx->config.mode == UNI_HAL_SPI_MODE_MASTER)) {
        SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
        if (instance != NULL) {
            const uint32_t clk_hz = _uni_hal_spi_clk_get(ctx->config.instance);
            const uint32_t prescaler_ll = LL_SPI_GetBaudRatePrescaler(instance);
            // BR encodes powers of two: 000 = PCLK/2 through 111 = PCLK/256.
            result = clk_hz / (2U << (prescaler_ll >> SPI_CR1_BR_Pos));
        }
    }

    return result;
}


bool uni_hal_spi_abort(uni_hal_spi_context_t *ctx) {
    // this driver has no asynchronous transfers, so there is never one to stop
    (void)ctx;
    return false;
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

            bool const is_master = ctx->config.mode == UNI_HAL_SPI_MODE_MASTER;
            bool stalled = false;

            for (size_t idx = 0; idx < len && !stalled; idx++) {
                uint32_t start_ms = uni_hal_systick_get_ms();
                while (!stalled && !LL_SPI_IsActiveFlag_TXE(instance)) {
                    stalled = is_master && (uni_hal_systick_get_ms() - start_ms) > UNI_HAL_SPI_STALL_TIMEOUT_MS;
                }
                if (stalled) {
                    break;
                }
                LL_SPI_TransmitData8(instance, tx_data ? tx_data[idx] : 0U);

                start_ms = uni_hal_systick_get_ms();
                while (!stalled && !LL_SPI_IsActiveFlag_RXNE(instance)) {
                    stalled = is_master && (uni_hal_systick_get_ms() - start_ms) > UNI_HAL_SPI_STALL_TIMEOUT_MS;
                }
                if (stalled) {
                    break;
                }
                data_rx = LL_SPI_ReceiveData8(instance);
                if (rx_data) {
                    rx_data[idx] = data_rx;
                }
            }

            // ES0335 2.22.2: BSY can stay set in slave mode. The final RXNE above
            // already signals completion of the last byte in full-duplex mode.
            if (is_master) {
                uint32_t const start_ms = uni_hal_systick_get_ms();
                while (!stalled && LL_SPI_IsActiveFlag_BSY(instance)) {
                    stalled = (uni_hal_systick_get_ms() - start_ms) > UNI_HAL_SPI_STALL_TIMEOUT_MS;
                }
            }

            LL_SPI_Disable(instance);
            result = !stalled;
        }
    }

    return result;
}

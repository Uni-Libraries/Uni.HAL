//
// Includes
//

// stdlib
#include <stddef.h>

// ST
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#include <stm32h7xx_ll_bus.h>
#include <stm32h7xx_ll_dma.h>
#include <stm32h7xx_ll_spi.h>
#pragma GCC diagnostic pop

#include <FreeRTOS.h>

// uni_hal
#include "core/uni_hal_core_cm7.h"
#include "rcc/uni_hal_rcc.h"
#include "spi/uni_hal_spi.h"
#include "core/uni_hal_core_cm7.h"


//
// Globals
//

uni_hal_spi_context_t *g_uni_hal_spi_ctx[6] = {NULL};



//
// Private
//

static void _uni_hal_spi_gpio_set_alternate(uni_hal_core_periph_e spi, uni_hal_gpio_pin_context_t *gpio) {
    uni_hal_gpio_alternate_e result = UNI_HAL_GPIO_ALTERNATE_0;

    if (spi == UNI_HAL_CORE_PERIPH_SPI_3) {
        if (gpio->gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_A) {
            switch (gpio->gpio_pin) {
                case UNI_HAL_GPIO_PIN_4:
                case UNI_HAL_GPIO_PIN_15:
                    result = UNI_HAL_GPIO_ALTERNATE_6;
                    break;
                default:
                    break;
            }
        } else if (gpio->gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_B) {
            switch (gpio->gpio_pin) {
                case UNI_HAL_GPIO_PIN_3:
                case UNI_HAL_GPIO_PIN_4:
                    result = UNI_HAL_GPIO_ALTERNATE_6;
                    break;
                case UNI_HAL_GPIO_PIN_2:
                case UNI_HAL_GPIO_PIN_5:
                    result = UNI_HAL_GPIO_ALTERNATE_7;
                    break;
                default:
                    break;
            }
        } else if (gpio->gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_C) {
            switch (gpio->gpio_pin) {
                case UNI_HAL_GPIO_PIN_10:
                case UNI_HAL_GPIO_PIN_11:
                case UNI_HAL_GPIO_PIN_12:
                    result = UNI_HAL_GPIO_ALTERNATE_6;
                    break;
                default:
                    break;
            }
        } else if (gpio->gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_D) {
            switch (gpio->gpio_pin) {
                case UNI_HAL_GPIO_PIN_6:
                    result = UNI_HAL_GPIO_ALTERNATE_5;
                    break;
                default:
                    break;
            }
        }
    } else if (spi == UNI_HAL_CORE_PERIPH_SPI_4) {
        if (gpio->gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_E) {
            switch (gpio->gpio_pin) {
                case UNI_HAL_GPIO_PIN_2:
                case UNI_HAL_GPIO_PIN_4:
                case UNI_HAL_GPIO_PIN_5:
                case UNI_HAL_GPIO_PIN_6:
                case UNI_HAL_GPIO_PIN_11:
                case UNI_HAL_GPIO_PIN_12:
                case UNI_HAL_GPIO_PIN_13:
                case UNI_HAL_GPIO_PIN_14:
                    result = UNI_HAL_GPIO_ALTERNATE_5;
                    break;
                default:
                    break;
            }
        }
    } else if (spi == UNI_HAL_CORE_PERIPH_SPI_5) {
        if (gpio->gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_F) {
            switch (gpio->gpio_pin) {
                case UNI_HAL_GPIO_PIN_6:
                case UNI_HAL_GPIO_PIN_7:
                case UNI_HAL_GPIO_PIN_8:
                case UNI_HAL_GPIO_PIN_9:
                case UNI_HAL_GPIO_PIN_11:
                    result = UNI_HAL_GPIO_ALTERNATE_5;
                    break;
                default:
                    break;
            }
        }
        else if (gpio->gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_H) {
            switch (gpio->gpio_pin) {
                case UNI_HAL_GPIO_PIN_5:
                case UNI_HAL_GPIO_PIN_6:
                case UNI_HAL_GPIO_PIN_7:
                    result = UNI_HAL_GPIO_ALTERNATE_5;
                    break;
                default:
                    break;
            }
        }
        else if (gpio->gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_J) {
            switch (gpio->gpio_pin) {
                case UNI_HAL_GPIO_PIN_10:
                case UNI_HAL_GPIO_PIN_11:
                    result = UNI_HAL_GPIO_ALTERNATE_5;
                    break;
                default:
                    break;
            }
        }
        else if (gpio->gpio_bank == UNI_HAL_CORE_PERIPH_GPIO_K) {
            switch (gpio->gpio_pin) {
                case UNI_HAL_GPIO_PIN_0:
                case UNI_HAL_GPIO_PIN_1:
                    result = UNI_HAL_GPIO_ALTERNATE_5;
                    break;
                default:
                    break;
            }
        }
    }

    if (result != UNI_HAL_GPIO_ALTERNATE_0) {
        gpio->alternate = result;
    }
}

static uint32_t _uni_hal_spi_dma_request_rx_get(uni_hal_core_periph_e instance) {
    size_t result = SIZE_MAX;
    switch (instance) {
        case UNI_HAL_CORE_PERIPH_SPI_1:
            result = LL_DMAMUX1_REQ_SPI1_RX;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_2:
            result = LL_DMAMUX1_REQ_SPI2_RX;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_3:
            result = LL_DMAMUX1_REQ_SPI3_RX;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_4:
            result = LL_DMAMUX1_REQ_SPI4_RX;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_5:
            result = LL_DMAMUX1_REQ_SPI5_RX;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_6:
            result = LL_DMAMUX2_REQ_SPI6_RX;
            break;
        default:
            break;
    }
    return result;
}

static uint32_t _uni_hal_spi_dma_request_tx_get(uni_hal_core_periph_e instance) {
    size_t result = SIZE_MAX;
    switch (instance) {
        case UNI_HAL_CORE_PERIPH_SPI_1:
            result = LL_DMAMUX1_REQ_SPI1_TX;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_2:
            result = LL_DMAMUX1_REQ_SPI2_TX;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_3:
            result = LL_DMAMUX1_REQ_SPI3_TX;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_4:
            result = LL_DMAMUX1_REQ_SPI4_TX;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_5:
            result = LL_DMAMUX1_REQ_SPI5_TX;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_6:
            result = LL_DMAMUX2_REQ_SPI6_TX;
            break;
        default:
            break;
    }
    return result;
}

static size_t _uni_hal_spi_index_get(uni_hal_core_periph_e instance) {
    size_t result = SIZE_MAX;
    switch (instance) {
        case UNI_HAL_CORE_PERIPH_SPI_1:
            result = 0; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_SPI_2:
            result = 1; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_SPI_3:
            result = 2; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_SPI_4:
            result = 3; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_SPI_5:
            result = 4; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_SPI_6:
            result = 5; //-V2571
            break;
        default:
            break;
    }
    return result;
}

static SPI_TypeDef *_uni_hal_spi_handle_get(uni_hal_core_periph_e instance) {
    SPI_TypeDef *result;
    switch (instance) {
        case UNI_HAL_CORE_PERIPH_SPI_1:
            result = SPI1; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_SPI_2:
            result = SPI2; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_SPI_3:
            result = SPI3; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_SPI_4:
            result = SPI4; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_SPI_5:
            result = SPI5; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_SPI_6:
            result = SPI6; //-V2571
            break;
        default:
            result = NULL;
            break;
    }
    return result;
}

static bool _uni_hal_spi_clear_flags(uni_hal_core_periph_e instance) {
    bool result = false;

    SPI_TypeDef *handle = _uni_hal_spi_handle_get(instance);
    if (handle != NULL) {
        LL_SPI_ClearFlag_OVR(handle);
        LL_SPI_ClearFlag_FRE(handle);
        LL_SPI_ClearFlag_CRCERR(handle);
        LL_SPI_ClearFlag_MODF(handle);
        LL_SPI_ClearFlag_EOT(handle);
        LL_SPI_ClearFlag_TXTF(handle);
        result = true;
    }

    return result;
}

/**
 * Convert SPI polarity to vendor speicific
 * @param polarity polarity enum value
 * @return converted value
 */
uint32_t _uni_hal_spi_phase(uni_hal_spi_cpha_e phase) {
    uint32_t result = 0;
    switch (phase) {
        case UNI_HAL_SPI_CPHA_0:
            result = LL_SPI_PHASE_1EDGE;
            break;
        case UNI_HAL_SPI_CPHA_1:
            result = LL_SPI_PHASE_2EDGE;
            break;
        default:
            // unknown phase
            break;
    }
    return result;
}


/**
 * Convert SPI polarity to vendor speicific
 * @param polarity polarity enum value
 * @return converted value
 */
uint32_t _uni_hal_spi_polarity(uni_hal_spi_cpol_e polarity) {
    uint32_t result = 0;
    switch (polarity) {
        case UNI_HAL_SPI_CPOL_0:
            result = LL_SPI_POLARITY_LOW;
            break;
        case UNI_HAL_SPI_CPOL_1:
            result = LL_SPI_POLARITY_HIGH;
            break;
        default:
            // unknown polarity
            break;
    }
    return result;
}


uint32_t _uni_hal_spi_prescaler(uni_hal_spi_prescaler_e prescaler) {
    uint32_t result = 0;
    switch (prescaler) {
        case UNI_HAL_SPI_PRESCALER_2:
            result = LL_SPI_BAUDRATEPRESCALER_DIV2;
            break;
        case UNI_HAL_SPI_PRESCALER_4:
            result = LL_SPI_BAUDRATEPRESCALER_DIV4;
            break;
        case UNI_HAL_SPI_PRESCALER_8:
            result = LL_SPI_BAUDRATEPRESCALER_DIV8;
            break;
        case UNI_HAL_SPI_PRESCALER_16:
            result = LL_SPI_BAUDRATEPRESCALER_DIV16;
            break;
        case UNI_HAL_SPI_PRESCALER_32:
            result = LL_SPI_BAUDRATEPRESCALER_DIV32;
            break;
        case UNI_HAL_SPI_PRESCALER_64:
            result = LL_SPI_BAUDRATEPRESCALER_DIV64;
            break;
        case UNI_HAL_SPI_PRESCALER_128:
            result = LL_SPI_BAUDRATEPRESCALER_DIV128;
            break;
        case UNI_HAL_SPI_PRESCALER_256:
            result = LL_SPI_BAUDRATEPRESCALER_DIV256;
            break;
        default:
            // unknown polarity
            break;
    }
    return result;
}


//
// Private/init
//

static bool _uni_hal_spi_init_rcc(uni_hal_spi_context_t *ctx) {
    return uni_hal_rcc_clksrc_set(ctx->config.instance, ctx->config.clock_source)
           && uni_hal_rcc_clk_set(ctx->config.instance, true);
}

static bool _uni_hal_spi_init_gpio(uni_hal_spi_context_t *ctx) {
    bool result = true;

    if (ctx->config.pin_sck != NULL) {
        ctx->config.pin_sck->gpio_pull = UNI_HAL_GPIO_PULL_NO;
        ctx->config.pin_sck->gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP;
        _uni_hal_spi_gpio_set_alternate(ctx->config.instance, ctx->config.pin_sck);
        result = uni_hal_gpio_pin_init(ctx->config.pin_sck) && result;
    }

    if (ctx->config.pin_miso != NULL) {
        ctx->config.pin_miso->gpio_pull = UNI_HAL_GPIO_PULL_NO;
        ctx->config.pin_miso->gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP;
        _uni_hal_spi_gpio_set_alternate(ctx->config.instance, ctx->config.pin_miso);
        result = uni_hal_gpio_pin_init(ctx->config.pin_miso) && result;
    }

    if (ctx->config.pin_mosi != NULL) {
        ctx->config.pin_mosi->gpio_pull = UNI_HAL_GPIO_PULL_NO;
        ctx->config.pin_mosi->gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP;
        _uni_hal_spi_gpio_set_alternate(ctx->config.instance, ctx->config.pin_mosi);
        result = uni_hal_gpio_pin_init(ctx->config.pin_mosi) && result;
    }

    if (ctx->config.pin_nss != NULL) {
        ctx->config.pin_nss->gpio_pull = UNI_HAL_GPIO_PULL_NO;
        ctx->config.pin_nss->gpio_init = true;
        ctx->config.pin_nss->gpio_type = ctx->config.nss_hard
                                             ? UNI_HAL_GPIO_TYPE_ALTERNATE_PP
                                             : UNI_HAL_GPIO_TYPE_OUT_PP;
        _uni_hal_spi_gpio_set_alternate(ctx->config.instance, ctx->config.pin_nss);
        result = uni_hal_gpio_pin_init(ctx->config.pin_nss) && result;
    }

    return result;
}


static bool _uni_hal_spi_init_irq(uni_hal_spi_context_t *ctx) {
    bool result = false;
    IRQn_Type irq = WWDG_IRQn;

    switch (ctx->config.instance) {
        case UNI_HAL_CORE_PERIPH_SPI_1:
            irq = SPI1_IRQn;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_2:
            irq = SPI2_IRQn;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_3:
            irq = SPI3_IRQn;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_4:
            irq = SPI4_IRQn;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_5:
            irq = SPI5_IRQn;
            break;
        case UNI_HAL_CORE_PERIPH_SPI_6:
            irq = SPI6_IRQn;
            break;
        default:
            break;
    }

    if (irq != WWDG_IRQn) {
        NVIC_SetPriority(irq, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
        NVIC_EnableIRQ(irq);
        result = true;
    }

    return result;
}


static bool _uni_hal_spi_init_dma(uni_hal_spi_context_t *ctx) {
    bool result = true;

    if (ctx->config.dma_tx != NULL) {
        result = result && uni_hal_dma_init(ctx->config.dma_tx);

        if (result) {
            uni_hal_dma_set_fifo_mode(ctx->config.dma_tx, false);
            uni_hal_dma_set_mode(ctx->config.dma_tx, UNI_HAL_DMA_MODE_NORMAL);
            uni_hal_dma_set_priority(ctx->config.dma_tx, UNI_HAL_DMA_PRIORITY_LOW);

            DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(ctx->config.dma_tx->config.instance);
            uint32_t stream = uni_hal_dma_stm32h7_get_channel(ctx->config.dma_tx->config.channel);

            LL_DMA_SetDataTransferDirection(module, stream, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
            LL_DMA_SetPeriphIncMode(module, stream, LL_DMA_PERIPH_NOINCREMENT);
            LL_DMA_SetMemoryIncMode(module, stream, LL_DMA_MEMORY_INCREMENT);
            LL_DMA_SetPeriphSize(module, stream, LL_DMA_PDATAALIGN_BYTE);
            LL_DMA_SetMemorySize(module, stream, LL_DMA_PDATAALIGN_BYTE);
            LL_DMA_SetPeriphRequest(module, stream, _uni_hal_spi_dma_request_tx_get(ctx->config.instance));

            LL_DMA_EnableIT_TC(module, stream);
            LL_DMA_EnableIT_TE(module, stream);
        }
    }

    if (ctx->config.dma_rx != NULL) {
        result = result && uni_hal_dma_init(ctx->config.dma_rx);

        if (result) {
            uni_hal_dma_set_fifo_mode(ctx->config.dma_rx, false);
            uni_hal_dma_set_mode(ctx->config.dma_rx, UNI_HAL_DMA_MODE_NORMAL);
            uni_hal_dma_set_priority(ctx->config.dma_rx, UNI_HAL_DMA_PRIORITY_LOW);

            DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(ctx->config.dma_rx->config.instance);
            uint32_t stream = uni_hal_dma_stm32h7_get_channel(ctx->config.dma_rx->config.channel);

            LL_DMA_SetDataTransferDirection(module, stream, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
            LL_DMA_SetPeriphIncMode(module, stream, LL_DMA_PERIPH_NOINCREMENT);
            LL_DMA_SetMemoryIncMode(module, stream, LL_DMA_MEMORY_INCREMENT);
            LL_DMA_SetPeriphSize(module, stream, LL_DMA_PDATAALIGN_BYTE);
            LL_DMA_SetMemorySize(module, stream, LL_DMA_PDATAALIGN_BYTE);
            LL_DMA_SetPeriphRequest(module, stream, _uni_hal_spi_dma_request_rx_get(ctx->config.instance));

            LL_DMA_EnableIT_TC(module, stream);
            LL_DMA_EnableIT_TE(module, stream);
        }
    }

    return result;
}


static bool _uni_hal_spi_init_spi(uni_hal_spi_context_t *ctx) {
    bool result = false;

    SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
    if (instance != NULL) {
        LL_SPI_InitTypeDef init_struct;
        LL_SPI_StructInit(&init_struct);
        init_struct.TransferDirection =  (ctx->config.mode == UNI_HAL_SPI_MODE_SLAVE) ? LL_SPI_SIMPLEX_RX : LL_SPI_FULL_DUPLEX;
        init_struct.Mode =  (ctx->config.mode == UNI_HAL_SPI_MODE_SLAVE) ? LL_SPI_MODE_SLAVE : LL_SPI_MODE_MASTER;
        init_struct.DataWidth =LL_SPI_DATAWIDTH_8BIT;
        init_struct.ClockPolarity =  _uni_hal_spi_polarity(ctx->config.polarity);
        init_struct.ClockPhase =  _uni_hal_spi_phase(ctx->config.phase);
        init_struct.NSS = LL_SPI_NSS_SOFT;
        if (ctx->config.nss_hard && ctx->config.pin_nss) {
            if (ctx->config.mode == UNI_HAL_SPI_MODE_MASTER) {
                init_struct.NSS = LL_SPI_NSS_HARD_OUTPUT;
            } else {
                init_struct.NSS = LL_SPI_NSS_HARD_INPUT;
            }
        }
        init_struct.BaudRate = _uni_hal_spi_prescaler(ctx->config.prescaler);
        init_struct.BitOrder = LL_SPI_MSB_FIRST;
        init_struct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
        if (ctx->config.crc_type != UNI_HAL_SPI_CRC_DISABLE) {
            init_struct.CRCCalculation = LL_SPI_CRCCALCULATION_ENABLE;
            init_struct.CRCPoly = ctx->config.crc_polynomial;
        }

        result = LL_SPI_Init(instance, &init_struct) == SUCCESS;
        if(result) {
            LL_SPI_SetFIFOThreshold(instance, LL_SPI_FIFO_TH_01DATA);
            LL_SPI_SetStandard(instance, LL_SPI_PROTOCOL_MOTOROLA);
            LL_SPI_SetNSSPolarity(instance, LL_SPI_NSS_POLARITY_LOW);

            if (ctx->config.crc_type != UNI_HAL_SPI_CRC_DISABLE)
            {
                LL_SPI_SetCRCWidth(instance, LL_SPI_CRC_16BIT);
                LL_SPI_SetTxCRCInitPattern(instance, LL_SPI_TXCRCINIT_ALL_ONES_PATTERN);
                LL_SPI_SetRxCRCInitPattern(instance, LL_SPI_TXCRCINIT_ALL_ONES_PATTERN);
            }
        }
    }

    return result;
}


//
// Public
//

bool uni_hal_spi_init(uni_hal_spi_context_t *ctx) {
    if (ctx != NULL && !uni_hal_spi_is_inited(ctx)) {
        bool result = _uni_hal_spi_init_rcc(ctx);
        result = result && _uni_hal_spi_init_gpio(ctx);
        result = result && _uni_hal_spi_init_irq(ctx);
        result = result && _uni_hal_spi_init_dma(ctx);
        result = result && _uni_hal_spi_init_spi(ctx);

        size_t index = _uni_hal_spi_index_get(ctx->config.instance);
        if (result && index != SIZE_MAX) {
            g_uni_hal_spi_ctx[index] = ctx;
            ctx->status.inited = true;
        }
    }

    return ctx->status.inited;
}


bool uni_hal_spi_is_inited(const uni_hal_spi_context_t *ctx) {
    return ctx != NULL && ctx->status.inited;
}


bool uni_hal_spi_is_busy(const uni_hal_spi_context_t *ctx) {
    return uni_hal_spi_is_inited(ctx) && ctx->status.in_process;
}


bool uni_hal_spi_receive(uni_hal_spi_context_t *ctx, uint8_t *data, uint32_t len) {
    return uni_hal_spi_transmitreceive(ctx, NULL, data, len);
}


bool uni_hal_spi_transceive_async(uni_hal_spi_context_t *ctx, const uint8_t *data_rx, const uint8_t *data_tx,
                                  uint32_t len) {
    bool result = false;
    if (uni_hal_spi_is_inited(ctx) && len > 0U && (data_rx || data_tx)) {
        ctx->status.in_process = true;
        ctx->status.last_rx_data = (uint8_t *) data_rx;
        ctx->status.last_len = len;

        if (!ctx->config.nss_hard) {
            uni_hal_gpio_pin_set(ctx->config.pin_nss, false);
        }

        SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);

        // set DMA source and destination
        if (data_rx != NULL) {
            uni_hal_core_cm7_dcache_invalidate((void *) data_rx, len);
            DMA_TypeDef *dma_rx_module = uni_hal_dma_stm32h7_get_module(ctx->config.dma_rx->config.instance);
            uint32_t dma_rx_stream = uni_hal_dma_stm32h7_get_channel(ctx->config.dma_rx->config.channel);

            LL_DMA_ConfigAddresses(dma_rx_module, dma_rx_stream, LL_SPI_DMA_GetRxRegAddr(instance), (uint32_t) data_rx,
                                   LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
            LL_DMA_SetDataLength(dma_rx_module, dma_rx_stream, len);
            LL_DMA_EnableStream(dma_rx_module, dma_rx_stream);

            LL_SPI_EnableDMAReq_RX(instance);
        }

        if (data_tx != NULL) {
            uni_hal_core_cm7_dcache_clean((void *) data_tx, len);
            DMA_TypeDef *dma_tx_module = uni_hal_dma_stm32h7_get_module(ctx->config.dma_tx->config.instance);
            uint32_t dma_tx_stream = uni_hal_dma_stm32h7_get_channel(ctx->config.dma_tx->config.channel);

            LL_DMA_ConfigAddresses(dma_tx_module, dma_tx_stream, (uint32_t) data_tx, LL_SPI_DMA_GetTxRegAddr(instance),
                                   LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
            LL_DMA_SetDataLength(dma_tx_module, dma_tx_stream, len);
            LL_DMA_EnableStream(dma_tx_module, dma_tx_stream);

            LL_SPI_EnableDMAReq_TX(instance);
        }

        // enable
        LL_SPI_DisableMasterRxAutoSuspend(instance);
        LL_SPI_SetTransferSize(instance, len);
        LL_SPI_EnableIT_EOT(instance);
        if (ctx->config.crc_type != UNI_HAL_SPI_CRC_DISABLE)
        {
            LL_SPI_EnableCRC(instance);
        }
        LL_SPI_Enable(instance);
        if (ctx->config.mode == UNI_HAL_SPI_MODE_MASTER) {
            LL_SPI_StartMasterTransfer(instance);
        }

        result = true;
    }
    return result;
}


bool uni_hal_spi_transmit(uni_hal_spi_context_t *ctx, const uint8_t *data, uint32_t len) {
    bool result = false;
    if (uni_hal_spi_is_inited(ctx) && data && len > 0U) {
        ctx->status.in_process = true;
        if (!ctx->config.nss_hard) {
            uni_hal_gpio_pin_set(ctx->config.pin_nss, false);
        }

        result = uni_hal_spi_transmitreceive(ctx, data, NULL, len);
        if (!ctx->config.nss_hard) {
            uni_hal_gpio_pin_set(ctx->config.pin_nss, true);
        }
        ctx->status.in_process = false;
    }
    return result;
}


bool uni_hal_spi_transmitreceive(uni_hal_spi_context_t *ctx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t len) {
    bool result = false;
    uint8_t data_rx;

    if (uni_hal_spi_is_inited(ctx) != false) {
        SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
        if (instance != NULL) {
            _uni_hal_spi_clear_flags(ctx->config.instance);

            if (ctx->config.crc_type != UNI_HAL_SPI_CRC_DISABLE)
            {
                LL_SPI_EnableCRC(instance);
            }

            LL_SPI_DisableGPIOControl(instance);
            LL_SPI_DisableMasterRxAutoSuspend(instance);
            LL_SPI_SetTransferSize(instance, len);

            LL_SPI_Enable(instance);
            if (ctx->config.mode == UNI_HAL_SPI_MODE_MASTER) {
                LL_SPI_StartMasterTransfer(instance);
            }


            size_t idx_tx = 0U;
            size_t idx_rx = 0U;
            while (idx_tx < len) {
                if (LL_SPI_IsActiveFlag_TXP(instance) != 0U) {
                    LL_SPI_TransmitData8(instance, (tx_data != NULL) ? tx_data[idx_tx] : (uint8_t) 0U);
                    idx_tx++;
                }
                if (idx_rx < len && (LL_SPI_IsActiveFlag_RXP(instance) != 0U)) {
                    data_rx = LL_SPI_ReceiveData8(instance);
                    if (rx_data != NULL) {
                        rx_data[idx_rx] = data_rx;
                    }
                    idx_rx++;
                }
            }

            while (LL_SPI_IsActiveFlag_EOT(instance) == 0U) {
            }
            LL_SPI_Disable(instance);
            _uni_hal_spi_clear_flags(ctx->config.instance);

            result = true;
        }
    }

    return result;
}


bool uni_hal_spi_set_prescaler(uni_hal_spi_context_t *ctx, uni_hal_spi_prescaler_e prescaler) {
    bool result = false;
    if (uni_hal_spi_is_inited(ctx) && ctx->config.prescaler != prescaler) {
        ctx->config.prescaler = prescaler;
        LL_SPI_SetBaudRatePrescaler(_uni_hal_spi_handle_get(ctx->config.instance), _uni_hal_spi_prescaler(prescaler));
        result = true;
    }
    return result;
}




//
// IRQ
//

bool SPIx_IRQHandler(uni_hal_spi_context_t *ctx, SPI_TypeDef *instance) {
    bool high_priority_woken = false;

    if (!ctx->config.nss_hard) {
        uni_hal_gpio_pin_set(ctx->config.pin_nss, true);
    }

    if(ctx->status.last_rx_data != NULL) {
        uni_hal_core_cm7_dcache_invalidate(ctx->status.last_rx_data, ctx->status.last_len);
    }

    LL_SPI_ClearFlag_EOT(instance);
    LL_SPI_ClearFlag_TXTF(instance);
    LL_SPI_ClearFlag_UDR(instance);
    LL_SPI_ClearFlag_CRCERR(instance);

    LL_SPI_Disable(instance);

    /* ES0392 #2.22.4: mask TXP/TXC while disabled */
    LL_SPI_DisableIT_TXP(instance);
    LL_SPI_DisableIT_EOT(instance);

    LL_SPI_DisableDMAReq_RX(instance);
    LL_SPI_DisableDMAReq_TX(instance);

    ctx->status.in_process = false;
    if(ctx->status.callback) {
        high_priority_woken = ctx->status.callback(ctx->status.callback_cookie);
    }

    return high_priority_woken;
}


void SPI1_IRQHandler() {
    traceISR_ENTER();
    portYIELD_FROM_ISR(SPIx_IRQHandler(g_uni_hal_spi_ctx[0], SPI1));
}

void SPI2_IRQHandler() {
    traceISR_ENTER();
    portYIELD_FROM_ISR(SPIx_IRQHandler(g_uni_hal_spi_ctx[1], SPI2));
}

void SPI3_IRQHandler() {
    traceISR_ENTER();
    portYIELD_FROM_ISR(SPIx_IRQHandler(g_uni_hal_spi_ctx[2], SPI3));
}

void SPI4_IRQHandler() {
    traceISR_ENTER();
    portYIELD_FROM_ISR(SPIx_IRQHandler(g_uni_hal_spi_ctx[3], SPI4));
}

void SPI5_IRQHandler() {
    traceISR_ENTER();
    portYIELD_FROM_ISR(SPIx_IRQHandler(g_uni_hal_spi_ctx[4], SPI5));
}

void SPI6_IRQHandler() {
    traceISR_ENTER();
    portYIELD_FROM_ISR(SPIx_IRQHandler(g_uni_hal_spi_ctx[5], SPI6));
}

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

// uni_hal
#include "rcc/uni_hal_rcc.h"
#include "spi/uni_hal_spi.h"


//
// Globals
//

uni_hal_spi_context_t *g_uni_hal_spi_ctx[6] = {nullptr};


//
// IRQ
//

void SPIx_IRQHandler(uni_hal_spi_context_t *ctx, SPI_TypeDef *instance) {
    if (!ctx->config.nss_hard) {
        uni_hal_gpio_pin_set(ctx->config.pin_nss, true);
    }

    LL_SPI_Disable(instance);

    LL_SPI_ClearFlag_EOT(instance);
    LL_SPI_ClearFlag_TXTF(instance);

    LL_SPI_DisableDMAReq_RX(instance);
    LL_SPI_DisableDMAReq_TX(instance);

    ctx->status.in_process = false;
}


void SPI1_IRQHandler() {
    SPIx_IRQHandler(g_uni_hal_spi_ctx[0], SPI1);
}

void SPI2_IRQHandler() {
    SPIx_IRQHandler(g_uni_hal_spi_ctx[1], SPI2);
}

void SPI3_IRQHandler() {
    SPIx_IRQHandler(g_uni_hal_spi_ctx[2], SPI3);
}

void SPI4_IRQHandler() {
    SPIx_IRQHandler(g_uni_hal_spi_ctx[3], SPI4);
}

void SPI5_IRQHandler() {
    SPIx_IRQHandler(g_uni_hal_spi_ctx[4], SPI5);
}

void SPI6_IRQHandler() {
    SPIx_IRQHandler(g_uni_hal_spi_ctx[5], SPI6);
}


//
// Private
//

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
            result = nullptr;
            break;
    }
    return result;
}

static bool _uni_hal_spi_clear_flags(uni_hal_core_periph_e instance) {
    bool result = false;

    SPI_TypeDef *handle = _uni_hal_spi_handle_get(instance);
    if (handle != nullptr) {
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
// Public
//

bool uni_hal_spi_init(uni_hal_spi_context_t *ctx) {
    bool result = false;

    if (ctx != nullptr && !uni_hal_spi_is_inited(ctx)) {
        size_t index = _uni_hal_spi_index_get(ctx->config.instance);
        if (index != SIZE_MAX) {
            g_uni_hal_spi_ctx[index] = ctx;
        }

        SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
        if (instance != nullptr) {
            // clock
            result = uni_hal_rcc_clksrc_set(ctx->config.instance, ctx->config.clock_source);
            result = uni_hal_rcc_clk_set(ctx->config.instance, true) && result;

            // gpio
            if (ctx->config.pin_sck != nullptr) {
                ctx->config.pin_sck->alternate = UNI_HAL_GPIO_ALTERNATE_5;
                ctx->config.pin_sck->gpio_pull = UNI_HAL_GPIO_PULL_NO;
                ctx->config.pin_sck->gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP;
                result = uni_hal_gpio_pin_init(ctx->config.pin_sck) && result;
            }
            if (ctx->config.pin_miso != nullptr) {
                ctx->config.pin_miso->alternate = UNI_HAL_GPIO_ALTERNATE_5;
                ctx->config.pin_miso->gpio_pull = UNI_HAL_GPIO_PULL_NO;
                ctx->config.pin_miso->gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP;
                result = uni_hal_gpio_pin_init(ctx->config.pin_miso) && result;
            }
            if (ctx->config.pin_mosi != nullptr) {
                ctx->config.pin_mosi->alternate = UNI_HAL_GPIO_ALTERNATE_5;
                ctx->config.pin_mosi->gpio_pull = UNI_HAL_GPIO_PULL_NO;
                ctx->config.pin_mosi->gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP;
                result = uni_hal_gpio_pin_init(ctx->config.pin_mosi) && result;
            }
            if (ctx->config.pin_nss != nullptr) {
                ctx->config.pin_nss->alternate = UNI_HAL_GPIO_ALTERNATE_5;
                ctx->config.pin_nss->gpio_pull = UNI_HAL_GPIO_PULL_NO;
                ctx->config.pin_nss->gpio_init = true;
                ctx->config.pin_nss->gpio_type = ctx->config.nss_hard
                                                     ? UNI_HAL_GPIO_TYPE_ALTERNATE_PP
                                                     : UNI_HAL_GPIO_TYPE_OUT_PP;
                result = uni_hal_gpio_pin_init(ctx->config.pin_nss) && result;
            }

            // irq
            IRQn_Type irq;
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
                    irq = SPI1_IRQn;
                    break;
            }
            NVIC_SetPriority(irq, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
            NVIC_EnableIRQ(irq);

            // dma
            if (ctx->config.dma_tx != nullptr) {
                result = uni_hal_dma_init(ctx->config.dma_tx) && result;

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
            if (ctx->config.dma_rx != nullptr) {
                result = uni_hal_dma_init(ctx->config.dma_rx) && result;

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

            // spi
            LL_SPI_InitTypeDef SPI_InitStruct = {0};
            LL_SPI_StructInit(&SPI_InitStruct);

            SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX; //-V1048
            SPI_InitStruct.Mode = ctx->config.mode == UNI_HAL_SPI_MODE_SLAVE ? LL_SPI_MODE_SLAVE : LL_SPI_MODE_MASTER;
            SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
            SPI_InitStruct.ClockPolarity = _uni_hal_spi_polarity(ctx->config.polarity);
            SPI_InitStruct.ClockPhase = _uni_hal_spi_phase(ctx->config.phase);
            SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
            if(ctx->config.nss_hard && ctx->config.pin_nss) {
                SPI_InitStruct.NSS = ctx->config.mode == UNI_HAL_SPI_MODE_SLAVE  ? LL_SPI_NSS_HARD_INPUT : LL_SPI_NSS_HARD_OUTPUT;
            }
            SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV2;
            SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
            SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;

            LL_SPI_SetFIFOThreshold(instance, LL_SPI_FIFO_TH_01DATA);
            result = (LL_SPI_Init(instance, &SPI_InitStruct) == SUCCESS) && result;
            if (result != false) {
                LL_SPI_SetStandard(instance, LL_SPI_PROTOCOL_MOTOROLA);
                if (ctx->config.pin_nss != nullptr) {
                    LL_SPI_EnableNSSPulseMgt(instance);
                    LL_SPI_SetNSSPolarity(instance, LL_SPI_NSS_POLARITY_LOW);
                } else {
                    LL_SPI_DisableNSSPulseMgt(instance);
                }
            }

            ctx->status.inited = true;
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

bool uni_hal_spi_is_busy(const uni_hal_spi_context_t *ctx) {
    return uni_hal_spi_is_inited(ctx) && ctx->status.in_process;
}


bool uni_hal_spi_receive(uni_hal_spi_context_t *ctx, uint8_t *data, uint32_t len) {
    return uni_hal_spi_transmitreceive(ctx, NULL, data, len);
}



bool uni_hal_spi_receive_async(uni_hal_spi_context_t *ctx, const uint8_t *data, uint32_t len) {
    bool result = false;
    if (uni_hal_spi_is_inited(ctx) && data && len > 0U) {
        ctx->status.in_process = true;
        if (!ctx->config.nss_hard) {
            uni_hal_gpio_pin_set(ctx->config.pin_nss, false);
        }

        // get instances
        SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
        DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(ctx->config.dma_rx->config.instance);
        uint32_t stream = uni_hal_dma_stm32h7_get_channel(ctx->config.dma_rx->config.channel);

        // set DMA source and destination
        LL_DMA_ConfigAddresses(module, stream, LL_SPI_DMA_GetRxRegAddr(instance), (uint32_t) data,
                               LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
        LL_DMA_SetDataLength(module, stream, len);
        LL_DMA_EnableStream(module, stream);

        // set SPI size
        LL_SPI_SetTransferSize(instance, len);

        // enable transfer
        LL_SPI_EnableDMAReq_RX(instance);
        LL_SPI_EnableIT_EOT(instance);
        LL_SPI_Enable(instance);

        result = true;
    }
    return result;
}


bool uni_hal_spi_transceive_async(uni_hal_spi_context_t *ctx, const uint8_t *data_rx, const uint8_t *data_tx, uint32_t len) {
    bool result = false;
    if (uni_hal_spi_is_inited(ctx) && len > 0U && (data_rx || data_tx)) {
        ctx->status.in_process = true;
        if (!ctx->config.nss_hard) {
            uni_hal_gpio_pin_set(ctx->config.pin_nss, false);
        }

        SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);

        // set DMA source and destination
        if(data_rx != NULL) {
            DMA_TypeDef *dma_rx_module = uni_hal_dma_stm32h7_get_module(ctx->config.dma_rx->config.instance);
            uint32_t dma_rx_stream = uni_hal_dma_stm32h7_get_channel(ctx->config.dma_rx->config.channel);

            LL_DMA_ConfigAddresses(dma_rx_module, dma_rx_stream, LL_SPI_DMA_GetRxRegAddr(instance), (uint32_t)data_rx,
                                      LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
            LL_DMA_SetDataLength(dma_rx_module, dma_rx_stream, len);
            LL_DMA_EnableStream(dma_rx_module, dma_rx_stream);

            LL_SPI_EnableDMAReq_RX(instance);
        }

        if(data_tx != NULL) {
            DMA_TypeDef *dma_tx_module = uni_hal_dma_stm32h7_get_module(ctx->config.dma_tx->config.instance);
            uint32_t dma_tx_stream = uni_hal_dma_stm32h7_get_channel(ctx->config.dma_tx->config.channel);

            LL_DMA_ConfigAddresses(dma_tx_module, dma_tx_stream, (uint32_t) data_tx, LL_SPI_DMA_GetTxRegAddr(instance),
                                   LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
            LL_DMA_SetDataLength(dma_tx_module, dma_tx_stream, len);
            LL_DMA_EnableStream(dma_tx_module, dma_tx_stream);

            LL_SPI_EnableDMAReq_TX(instance);
        }

        // enable
        LL_SPI_SetTransferSize(instance, len);
        LL_SPI_EnableIT_EOT(instance);
        LL_SPI_Enable(instance);
        if(ctx->config.mode == UNI_HAL_SPI_MODE_MASTER) {
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

            LL_SPI_DisableGPIOControl(instance);
            LL_SPI_DisableMasterRxAutoSuspend(instance);
            LL_SPI_SetTransferSize(instance, len);

            LL_SPI_Enable(instance);
            LL_SPI_StartMasterTransfer(instance);


            size_t idx_tx = 0U;
            size_t idx_rx = 0U;
            while (idx_tx < len) {
                if (LL_SPI_IsActiveFlag_TXP(instance) != 0U) {
                    LL_SPI_TransmitData8(instance, (tx_data != nullptr) ? tx_data[idx_tx] : (uint8_t) 0U);
                    idx_tx++;
                }
                if (idx_rx < len && (LL_SPI_IsActiveFlag_RXP(instance) != 0U)) {
                    data_rx = LL_SPI_ReceiveData8(instance);
                    if (rx_data != nullptr) {
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


bool uni_hal_spi_set_prescaler(const uni_hal_spi_context_t *ctx, uni_hal_spi_prescaler_e prescaler) {
    bool result = false;
    if (uni_hal_spi_is_inited(ctx)) {
        LL_SPI_SetBaudRatePrescaler(_uni_hal_spi_handle_get(ctx->config.instance), _uni_hal_spi_prescaler(prescaler));
        result = true;
    }
    return result;
}

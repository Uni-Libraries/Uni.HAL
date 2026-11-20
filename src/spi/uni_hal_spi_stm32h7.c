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

// Uni.HAL OS layer
#include "os/uni_hal_os.h"

// uni_hal
#include "core/uni_hal_core_cm7.h"
#include "rcc/uni_hal_rcc.h"
#include "spi/uni_hal_spi.h"
#include "systick/uni_hal_systick.h"
#include "dma/uni_hal_dma_stm32h7_irq.h"


//
// Globals
//

static uni_hal_spi_context_t *g_uni_hal_spi_ctx[6];
typedef struct {
    uni_hal_dma_context_t *rx;
    uni_hal_dma_context_t *tx;
    uint8_t pending;
    bool active;
    bool aborting;
    bool awaiting_suspend;
} spi_transfer_t;
static spi_transfer_t transfers[6];
// One SPI IRQ delivers its result until its callback returns. A preempting IRQ must not erase it.
static volatile uint32_t callback_level[6];

static bool reserved_for_callback(size_t index) {
    return callback_level[index] && callback_level[index] != __get_IPSR() + 1U;
}
static const IRQn_Type spi_irqs[] = {SPI1_IRQn, SPI2_IRQn, SPI3_IRQn, SPI4_IRQn, SPI5_IRQn, SPI6_IRQn};
#define SPI_RX_PENDING 1U
#define SPI_TX_PENDING 2U
#define SPI_ERROR_IRQS (SPI_IER_UDRIE | SPI_IER_OVRIE | SPI_IER_CRCEIE | SPI_IER_TIFREIE | SPI_IER_MODFIE)


/**
 * A blocking master transfer is abandoned when the bus makes no progress for this long.
 * A slave waits for its master for as long as it takes.
 */
#define UNI_HAL_SPI_STALL_TIMEOUT_MS (100U)



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
        LL_SPI_ClearFlag_UDR(handle);
        LL_SPI_ClearFlag_SUSP(handle);
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

static uint32_t _uni_hal_spi_prescaler_div_get(uint32_t prescaler_ll) {
    uint32_t result = 0U;
    switch (prescaler_ll) {
        case LL_SPI_BAUDRATEPRESCALER_DIV2:
            result = 2U;
            break;
        case LL_SPI_BAUDRATEPRESCALER_DIV4:
            result = 4U;
            break;
        case LL_SPI_BAUDRATEPRESCALER_DIV8:
            result = 8U;
            break;
        case LL_SPI_BAUDRATEPRESCALER_DIV16:
            result = 16U;
            break;
        case LL_SPI_BAUDRATEPRESCALER_DIV32:
            result = 32U;
            break;
        case LL_SPI_BAUDRATEPRESCALER_DIV64:
            result = 64U;
            break;
        case LL_SPI_BAUDRATEPRESCALER_DIV128:
            result = 128U;
            break;
        case LL_SPI_BAUDRATEPRESCALER_DIV256:
            result = 256U;
            break;
        default:
            // unknown prescaler
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
    size_t const index = _uni_hal_spi_index_get(ctx->config.instance);
    if (index == SIZE_MAX) {
        return false;
    }
    IRQn_Type const irq = spi_irqs[index];
    NVIC_SetPriority(irq, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(irq);
    return true;
}

static bool valid_dma(const uni_hal_dma_context_t *dma) {
    return dma == nullptr || (uni_hal_dma_stm32h7_get_module(dma->config.instance) != nullptr &&
                              (unsigned)dma->config.channel <= UNI_HAL_DMA_CHANNEL_7);
}

static void configure_stream(uni_hal_spi_context_t *ctx, uni_hal_dma_context_t *dma, bool transmit,
                             uint32_t address, uint32_t length) {
    SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
    DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(dma->config.instance);
    uint32_t const stream = dma->config.channel;
    uint32_t const direction = transmit ? LL_DMA_DIRECTION_MEMORY_TO_PERIPH : LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
    LL_DMA_DisableFifoMode(module, stream);
    LL_DMA_SetMode(module, stream, LL_DMA_MODE_NORMAL);
    LL_DMA_SetStreamPriorityLevel(module, stream, LL_DMA_PRIORITY_LOW);
    LL_DMA_SetDataTransferDirection(module, stream, direction);
    LL_DMA_SetPeriphIncMode(module, stream, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(module, stream, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(module, stream, LL_DMA_PDATAALIGN_BYTE);
    LL_DMA_SetMemorySize(module, stream, LL_DMA_MDATAALIGN_BYTE);
    LL_DMA_SetPeriphRequest(module, stream, transmit ? _uni_hal_spi_dma_request_tx_get(ctx->config.instance)
                                                    : _uni_hal_spi_dma_request_rx_get(ctx->config.instance));
    LL_DMA_ConfigAddresses(module, stream, transmit ? address : LL_SPI_DMA_GetRxRegAddr(instance),
                           transmit ? LL_SPI_DMA_GetTxRegAddr(instance) : address, direction);
    LL_DMA_SetDataLength(module, stream, length);
    LL_DMA_EnableIT_TC(module, stream);
    LL_DMA_EnableIT_TE(module, stream);
    LL_DMA_EnableIT_DME(module, stream);
    LL_DMA_EnableStream(module, stream);
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

        result = LL_SPI_Init(instance, &init_struct) == SUCCESS;
        if(result) {
            LL_SPI_SetFIFOThreshold(instance, LL_SPI_FIFO_TH_01DATA);
            LL_SPI_SetStandard(instance, LL_SPI_PROTOCOL_MOTOROLA);
            LL_SPI_SetNSSPolarity(instance, LL_SPI_NSS_POLARITY_LOW);
            if (init_struct.NSS == LL_SPI_NSS_SOFT) {
                LL_SPI_SetInternalSSLevel(instance, ctx->config.mode == UNI_HAL_SPI_MODE_MASTER
                                                       ? LL_SPI_SS_LEVEL_HIGH : LL_SPI_SS_LEVEL_LOW);
            }

            if (ctx->config.crc_type != UNI_HAL_SPI_CRC_DISABLE)
            {
                LL_SPI_EnableCRC(instance);
                LL_SPI_EnableFullSizeCRC(instance);
                LL_SPI_SetCRCWidth(instance, LL_SPI_CRC_16BIT);

                const uint32_t tx_init = (ctx->config.crc_init == 0xFFFFU)
                                            ? LL_SPI_TXCRCINIT_ALL_ONES_PATTERN
                                            : LL_SPI_TXCRCINIT_ALL_ZERO_PATTERN;
                const uint32_t rx_init = (ctx->config.crc_init == 0xFFFFU)
                                            ? LL_SPI_RXCRCINIT_ALL_ONES_PATTERN
                                            : LL_SPI_RXCRCINIT_ALL_ZERO_PATTERN;

                LL_SPI_SetTxCRCInitPattern(instance, tx_init);
                LL_SPI_SetRxCRCInitPattern(instance, rx_init);

                LL_SPI_SetCRCPolynomial(instance, ctx->config.crc_polynomial);
            }
        }
    }

    return result;
}


//
// Public
//

bool uni_hal_spi_init(uni_hal_spi_context_t *ctx) {
    if (!ctx) {
        return false;
    }
    size_t const index = _uni_hal_spi_index_get(ctx->config.instance);
    uni_hal_dma_context_t *rx = ctx->config.dma_rx;
    uni_hal_dma_context_t *tx = ctx->config.dma_tx;
    if (index == SIZE_MAX || !valid_dma(rx) || !valid_dma(tx) ||
        (index == 5U && (rx || tx))) { // SPI6 is routed to BDMA/DMAMUX2.
        return false;
    }
    uint32_t mask = uni_hal_core_irq_pause();
    uni_hal_spi_context_t *old = g_uni_hal_spi_ctx[index];
    if (old == ctx || (old && (!old->status.inited || old->status.in_process || callback_level[index]))) {
        bool const ready = old == ctx && ctx->status.inited;
        uni_hal_core_irq_resume(mask);
        return ready;
    }
    if (old) {
        old->status.inited = false;
    }
    ctx->status.inited = false;
    g_uni_hal_spi_ctx[index] = ctx;
    uni_hal_core_irq_resume(mask);
    bool const ready = _uni_hal_spi_init_rcc(ctx) && _uni_hal_spi_init_gpio(ctx) &&
                       (!rx || uni_hal_dma_init(rx)) && (!tx || uni_hal_dma_init(tx)) &&
                       _uni_hal_spi_init_spi(ctx) && _uni_hal_spi_init_irq(ctx);
    mask = uni_hal_core_irq_pause();
    ctx->status.inited = ready;
    if (!ready) {
        g_uni_hal_spi_ctx[index] = nullptr;
    }
    uni_hal_core_irq_resume(mask);
    return ready;
}


bool uni_hal_spi_is_inited(const uni_hal_spi_context_t *ctx) {
    if (!ctx || !ctx->status.inited) {
        return false;
    }
    size_t const index = _uni_hal_spi_index_get(ctx->config.instance);
    return index != SIZE_MAX && g_uni_hal_spi_ctx[index] == ctx;
}


bool uni_hal_spi_is_busy(const uni_hal_spi_context_t *ctx) {
    return uni_hal_spi_is_inited(ctx) &&
           (ctx->status.in_process || reserved_for_callback(_uni_hal_spi_index_get(ctx->config.instance)));
}


uint32_t uni_hal_spi_bitrate_get(uni_hal_spi_context_t *ctx) {
    uint32_t result = 0U;

    if (uni_hal_spi_is_inited(ctx) && (ctx->config.mode == UNI_HAL_SPI_MODE_MASTER)) {
        SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
        if (instance != NULL) {
            const uint32_t clk_hz = uni_hal_rcc_clk_get_freq(ctx->config.instance);
            const uint32_t prescaler_ll = LL_SPI_GetBaudRatePrescaler(instance);
            const uint32_t div = _uni_hal_spi_prescaler_div_get(prescaler_ll);
            if (clk_hz > 0U && div > 0U) {
                result = clk_hz / div;
            }
        }
    }

    return result;
}


bool uni_hal_spi_receive(uni_hal_spi_context_t *ctx, uint8_t *data, uint32_t len) {
    return uni_hal_spi_transmitreceive(ctx, NULL, data, len);
}


// Runs from the DMA IRQ with interrupts masked. Application callbacks always run from the SPI IRQ.
static void dma_event(void *owner, const uni_hal_dma_context_t *dma, bool error) {
    uni_hal_spi_context_t *ctx = owner;
    size_t const index = _uni_hal_spi_index_get(ctx->config.instance);
    if (index == SIZE_MAX) {
        return;
    }
    spi_transfer_t *transfer = &transfers[index];
    if (!transfer->active) {
        if (error && (dma == transfer->rx || dma == transfer->tx)) {
            ctx->status.last_errors |= UNI_HAL_SPI_ERROR_DMA;
        }
        return;
    }
    transfer->pending &= ~(dma == transfer->rx ? SPI_RX_PENDING : SPI_TX_PENDING);
    DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(dma->config.instance);
    if (error || (!transfer->aborting && !ctx->status.last_errors && LL_DMA_GetDataLength(module, dma->config.channel) != 0U)) {
        ctx->status.last_errors |= UNI_HAL_SPI_ERROR_DMA;
    }
    if (ctx->status.last_errors || transfer->aborting) {
        NVIC_SetPendingIRQ(spi_irqs[index]);
    } else if (!transfer->pending) {
        // EOT stays latched even when the last DMA write happens after the last SPI clock.
        LL_SPI_EnableIT_EOT(_uni_hal_spi_handle_get(ctx->config.instance));
    }
}


bool uni_hal_spi_transceive_async(uni_hal_spi_context_t *ctx, uint8_t *data_rx, const uint8_t *data_tx,
                                  uint32_t len) {
    if (!uni_hal_spi_is_inited(ctx) || len == 0U || len > UINT16_MAX || (!data_tx && !data_rx) ||
        (data_rx && !ctx->config.dma_rx) || (data_tx && !ctx->config.dma_tx)) {
        return false;
    }
    if (data_rx && data_tx && data_rx != data_tx) {
        uintptr_t const rx_addr = (uintptr_t)data_rx;
        uintptr_t const tx_addr = (uintptr_t)data_tx;
        uintptr_t const distance = rx_addr > tx_addr ? rx_addr - tx_addr : tx_addr - rx_addr;
        if (distance < len) {
            return false;
        }
    }
    size_t const index = _uni_hal_spi_index_get(ctx->config.instance);
    SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
    spi_transfer_t *transfer = &transfers[index];
    uni_hal_dma_context_t *rx = data_rx ? ctx->config.dma_rx : nullptr;
    uni_hal_dma_context_t *tx = data_tx ? ctx->config.dma_tx : nullptr;
    uint32_t mask = uni_hal_core_irq_pause();
    if (g_uni_hal_spi_ctx[index] != ctx || !ctx->status.inited || ctx->status.in_process ||
        reserved_for_callback(index) || (rx && !uni_hal_dma_stm32h7_claim(rx, dma_event, ctx))) {
        uni_hal_core_irq_resume(mask);
        return false;
    }
    if (tx && !uni_hal_dma_stm32h7_claim(tx, dma_event, ctx)) {
        if (rx) {
            uni_hal_dma_stm32h7_release(rx);
        }
        uni_hal_core_irq_resume(mask);
        return false;
    }
    ctx->status.in_process = true;
    uni_hal_core_irq_resume(mask);

    // Clean TX first, including when both directions use the same buffer.
    if (tx) {
        uni_hal_core_cm7_dcache_clean((void *)data_tx, len);
    }
    if (rx) {
        uni_hal_core_cm7_dcache_cleaninvalidate_addr(data_rx, len);
    }

    mask = uni_hal_core_irq_pause();
    *transfer = (spi_transfer_t){.rx = rx, .tx = tx, .active = true,
                                 .pending = (rx ? SPI_RX_PENDING : 0U) | (tx ? SPI_TX_PENDING : 0U)};
    ctx->status.last_rx_data = data_rx;
    ctx->status.last_len = len;
    ctx->status.last_errors = UNI_HAL_SPI_ERROR_NONE;
    _uni_hal_spi_clear_flags(ctx->config.instance);
    LL_SPI_SetTransferDirection(instance, rx && tx ? LL_SPI_FULL_DUPLEX : (tx ? LL_SPI_SIMPLEX_TX : LL_SPI_SIMPLEX_RX));
    LL_SPI_DisableMasterRxAutoSuspend(instance);
    LL_SPI_SetTransferSize(instance, len);
    if (rx) {
        configure_stream(ctx, rx, false, (uint32_t)data_rx, len);
        LL_SPI_EnableDMAReq_RX(instance);
    }
    if (tx) {
        configure_stream(ctx, tx, true, (uint32_t)data_tx, len);
        LL_SPI_EnableDMAReq_TX(instance);
    }
    if (!ctx->config.nss_hard) {
        uni_hal_gpio_pin_set(ctx->config.pin_nss, false);
    }
    SET_BIT(instance->IER, SPI_IER_MODFIE | SPI_IER_TIFREIE |
            (tx ? SPI_IER_UDRIE : 0U) | (rx ? SPI_IER_OVRIE | SPI_IER_CRCEIE : 0U));
    LL_SPI_Enable(instance);
    if (ctx->config.mode == UNI_HAL_SPI_MODE_MASTER) {
        LL_SPI_StartMasterTransfer(instance);
    }
    uni_hal_core_irq_resume(mask);
    return true;
}

static bool stop_stream(const uni_hal_dma_context_t *dma) {
    if (!dma) {
        return true;
    }
    DMA_TypeDef *module = uni_hal_dma_stm32h7_get_module(dma->config.instance);
    uint32_t const stream = dma->config.channel;
    LL_DMA_DisableStream(module, stream);
    // EN may stay high while the last bus beat finishes. Keep ownership until hardware releases it.
    return !LL_DMA_IsEnabledStream(module, stream);
}

// Called with IRQs masked. Until both EN bits clear, a DMA bus beat can still access the buffers.
static bool quiesce(uni_hal_spi_context_t *ctx, SPI_TypeDef *instance, spi_transfer_t *transfer) {
    CLEAR_BIT(instance->IER, SPI_IER_EOTIE | SPI_ERROR_IRQS | SPI_IER_TXPIE);
    LL_SPI_Disable(instance);
    LL_SPI_DisableDMAReq_RX(instance);
    LL_SPI_DisableDMAReq_TX(instance);
    bool const rx_stopped = stop_stream(transfer->rx);
    bool const tx_stopped = stop_stream(transfer->tx);
    if (!rx_stopped || !tx_stopped) {
        // Disabling an in-flight normal stream generates TC after the last bus beat.
        return false;
    }
    _uni_hal_spi_clear_flags(ctx->config.instance);
    transfer->active = false;
    return true;
}

// The SPI stays busy through cache maintenance; higher-priority interrupts can run during it.
static void return_buffers(uni_hal_spi_context_t *ctx) {
    if (ctx->status.last_rx_data) {
        uni_hal_core_cm7_dcache_invalidate(ctx->status.last_rx_data, ctx->status.last_len);
    }
    if (!ctx->config.nss_hard) {
        uni_hal_gpio_pin_set(ctx->config.pin_nss, true);
    }
    uint32_t const mask = uni_hal_core_irq_pause();
    spi_transfer_t *transfer = &transfers[_uni_hal_spi_index_get(ctx->config.instance)];
    bool const rx_error = transfer->rx && uni_hal_dma_stm32h7_release(transfer->rx);
    bool const tx_error = transfer->tx && uni_hal_dma_stm32h7_release(transfer->tx);
    if (rx_error || tx_error) {
        ctx->status.last_errors |= UNI_HAL_SPI_ERROR_DMA;
    }
    *transfer = (spi_transfer_t){};
    ctx->status.last_rx_data = nullptr;
    ctx->status.last_len = 0U;
    ctx->status.in_process = false;
    uni_hal_core_irq_resume(mask);
}

bool uni_hal_spi_abort(uni_hal_spi_context_t *ctx) {
    if (!uni_hal_spi_is_inited(ctx)) {
        return false;
    }
    size_t const index = _uni_hal_spi_index_get(ctx->config.instance);
    if (index == SIZE_MAX) {
        return false;
    }
    uint32_t const mask = uni_hal_core_irq_pause();
    spi_transfer_t *transfer = &transfers[index];
    bool stopped = false;
    if (g_uni_hal_spi_ctx[index] == ctx && transfer->active) {
        SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
        bool const first = !transfer->aborting;
        transfer->aborting = true;
        transfer->awaiting_suspend = false;
        if (first && ctx->config.mode == UNI_HAL_SPI_MODE_MASTER && (instance->CR1 & SPI_CR1_CSTART) &&
            !(instance->SR & (SPI_SR_EOT | SPI_SR_SUSP | SPI_SR_MODF))) {
            // Stop on a frame boundary. A second abort can force a stalled master to stop.
            LL_SPI_SuspendMasterTransfer(instance);
            if ((instance->CR1 & SPI_CR1_CSTART) && !(instance->SR & SPI_SR_SUSP)) {
                transfer->awaiting_suspend = true;
                CLEAR_BIT(instance->IER, SPI_ERROR_IRQS);
                LL_SPI_EnableIT_EOT(instance); // EOTIE also signals SUSP.
            }
        }
        if (!transfer->awaiting_suspend) {
            stopped = quiesce(ctx, instance, transfer);
        }
    }
    uni_hal_core_irq_resume(mask);
    if (stopped) {
        return_buffers(ctx);
    }
    return stopped;
}


static bool blocking_transfer(uni_hal_spi_context_t *ctx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t len,
                              bool drive_nss) {
    if (!uni_hal_spi_is_inited(ctx) || len == 0U || len > UINT16_MAX) {
        return false;
    }
    uint32_t const mask = uni_hal_core_irq_pause();
    if (!uni_hal_spi_is_inited(ctx) || uni_hal_spi_is_busy(ctx)) {
        uni_hal_core_irq_resume(mask);
        return false;
    }
    ctx->status.in_process = true;
    uni_hal_core_irq_resume(mask);
    if (drive_nss && !ctx->config.nss_hard) {
        uni_hal_gpio_pin_set(ctx->config.pin_nss, false);
    }
    SPI_TypeDef *instance = _uni_hal_spi_handle_get(ctx->config.instance);
    uint8_t data_rx;
    _uni_hal_spi_clear_flags(ctx->config.instance);
    LL_SPI_SetTransferDirection(instance, LL_SPI_FULL_DUPLEX);

    LL_SPI_DisableGPIOControl(instance);
    LL_SPI_DisableMasterRxAutoSuspend(instance);
    LL_SPI_SetTransferSize(instance, len);

    LL_SPI_Enable(instance);
    if (ctx->config.mode == UNI_HAL_SPI_MODE_MASTER) {
        LL_SPI_StartMasterTransfer(instance);
    }


    // every byte sent clocks one byte in: keep reading until the last one has arrived,
    // it shows up only after the last byte has been written to the TX FIFO
    size_t idx_tx = 0U;
    size_t idx_rx = 0U;
    bool const is_master = ctx->config.mode == UNI_HAL_SPI_MODE_MASTER;
    bool stalled = false;
    uint32_t activity_ms = uni_hal_systick_get_ms();
    while (!stalled && (idx_tx < len || idx_rx < len)) {
        bool progress = false;
        if (idx_tx < len && LL_SPI_IsActiveFlag_TXP(instance) != 0U) {
            LL_SPI_TransmitData8(instance, (tx_data != NULL) ? tx_data[idx_tx] : (uint8_t) 0U);
            idx_tx++;
            progress = true;
        }
        if (idx_rx < len && (LL_SPI_IsActiveFlag_RXP(instance) != 0U)) {
            data_rx = LL_SPI_ReceiveData8(instance);
            if (rx_data != NULL) {
                rx_data[idx_rx] = data_rx;
            }
            idx_rx++;
            progress = true;
        }

        if (progress) {
            activity_ms = uni_hal_systick_get_ms();
        }
        else if (is_master && (uni_hal_systick_get_ms() - activity_ms) > UNI_HAL_SPI_STALL_TIMEOUT_MS) {
            stalled = true;
        }
    }

    while (!stalled && LL_SPI_IsActiveFlag_EOT(instance) == 0U) {
        if (is_master && (uni_hal_systick_get_ms() - activity_ms) > UNI_HAL_SPI_STALL_TIMEOUT_MS) {
            stalled = true;
        }
    }
    LL_SPI_Disable(instance);
    _uni_hal_spi_clear_flags(ctx->config.instance);

    if (drive_nss && !ctx->config.nss_hard) {
        uni_hal_gpio_pin_set(ctx->config.pin_nss, true);
    }
    ctx->status.in_process = false;
    return !stalled;
}

bool uni_hal_spi_transmit(uni_hal_spi_context_t *ctx, const uint8_t *data, uint32_t len) {
    return data && blocking_transfer(ctx, data, nullptr, len, true);
}

bool uni_hal_spi_transmitreceive(uni_hal_spi_context_t *ctx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t len) {
    return blocking_transfer(ctx, tx_data, rx_data, len, false);
}


bool uni_hal_spi_set_prescaler(uni_hal_spi_context_t *ctx, uni_hal_spi_prescaler_e prescaler) {
    uint32_t const mask = uni_hal_core_irq_pause();
    bool result = false;
    if (uni_hal_spi_is_inited(ctx) && !uni_hal_spi_is_busy(ctx) && ctx->config.prescaler != prescaler) {
        ctx->config.prescaler = prescaler;
        LL_SPI_SetBaudRatePrescaler(_uni_hal_spi_handle_get(ctx->config.instance), _uni_hal_spi_prescaler(prescaler));
        result = true;
    }
    uni_hal_core_irq_resume(mask);
    return result;
}




//
// IRQ
//

bool SPIx_IRQHandler(uni_hal_spi_context_t *ctx, SPI_TypeDef *instance) {
    uint32_t const mask = uni_hal_core_irq_pause();
    if (!uni_hal_spi_is_inited(ctx)) {
        uni_hal_core_irq_resume(mask);
        return false;
    }
    size_t const index = _uni_hal_spi_index_get(ctx->config.instance);
    spi_transfer_t *transfer = &transfers[index];
    if (!transfer->active) {
        uni_hal_core_irq_resume(mask);
        return false;
    }
    uint32_t const status = instance->SR;
    if (transfer->awaiting_suspend && (instance->CR1 & SPI_CR1_CSTART) &&
        !(status & (SPI_SR_EOT | SPI_SR_SUSP | SPI_SR_MODF))) {
        uni_hal_core_irq_resume(mask);
        return false;
    }
    if (!transfer->aborting && (status & SPI_SR_SUSP)) {
        ctx->status.last_errors |= UNI_HAL_SPI_ERROR_SUSPENDED;
    }
    if (transfer->rx && (status & SPI_SR_CRCE)) {
        ctx->status.last_errors |= UNI_HAL_SPI_ERROR_CRC;
    }
    if (transfer->rx && (status & SPI_SR_OVR)) {
        ctx->status.last_errors |= UNI_HAL_SPI_ERROR_OVERRUN;
    }
    if (transfer->tx && (status & SPI_SR_UDR)) {
        ctx->status.last_errors |= UNI_HAL_SPI_ERROR_UNDERRUN;
    }
    if (status & SPI_SR_MODF) {
        ctx->status.last_errors |= UNI_HAL_SPI_ERROR_MODE_FAULT;
    }
    if (status & SPI_SR_TIFRE) {
        ctx->status.last_errors |= UNI_HAL_SPI_ERROR_FRAME;
    }
    bool const notify = !transfer->aborting;
    if (!(ctx->status.last_errors || transfer->aborting || (!transfer->pending && (status & SPI_SR_EOT))) ||
        !quiesce(ctx, instance, transfer)) {
        uni_hal_core_irq_resume(mask);
        return false;
    }
    uni_hal_spi_callback_t const callback = notify ? ctx->status.callback : nullptr;
    if (callback) {
        callback_level[index] = __get_IPSR() + 1U;
    }
    void *cookie = ctx->status.callback_cookie;
    uni_hal_core_irq_resume(mask);
    return_buffers(ctx);
    if (!callback) {
        return false;
    }
    bool const woken = callback(cookie);
    callback_level[index] = 0U;
    return woken;
}


void SPI1_IRQHandler() {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(SPIx_IRQHandler(g_uni_hal_spi_ctx[0], SPI1));
}

void SPI2_IRQHandler() {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(SPIx_IRQHandler(g_uni_hal_spi_ctx[1], SPI2));
}

void SPI3_IRQHandler() {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(SPIx_IRQHandler(g_uni_hal_spi_ctx[2], SPI3));
}

void SPI4_IRQHandler() {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(SPIx_IRQHandler(g_uni_hal_spi_ctx[3], SPI4));
}

void SPI5_IRQHandler() {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(SPIx_IRQHandler(g_uni_hal_spi_ctx[4], SPI5));
}

void SPI6_IRQHandler() {
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(SPIx_IRQHandler(g_uni_hal_spi_ctx[5], SPI6));
}

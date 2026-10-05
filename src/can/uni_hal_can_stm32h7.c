//
// Includes
//

// stdlib
#include <stddef.h>
#include <string.h>

// st
#include <stm32h7xx.h>

// uni_hal
#include "can/uni_hal_can.h"
#include "core/uni_hal_core.h"
#include "os/uni_hal_os.h"
#include "rcc/uni_hal_rcc.h"
#include "systick/uni_hal_systick.h"



//
// The STM32H7 has FDCAN (Bosch M_CAN) instead of the bxCAN of the STM32L4. It is driven through
// its registers: ST ships no LL driver for it. Register and bit names are those of RM0433,
// chapter "FD controller area network (FDCAN)".
//
// The driver offers the same interface as the bxCAN one, which shapes how FDCAN is used:
//
// - classic CAN frames only, up to 8 data bytes;
// - three TX buffers, which play the part of the three TX mailboxes;
// - acceptance filters are given in the bxCAN register layout and translated, see
//   uni_hal_can_set_filter().
//



//
// Defines
//

/**
 * Longest wait for one frame to leave its TX buffer
 */
#define UNI_HAL_CAN_TX_TIMEOUT_MS (100U)

/**
 * Longest wait for a mode change. Joining the bus needs 11 consecutive recessive bits.
 */
#define UNI_HAL_CAN_MODE_TIMEOUT_MS (10U)

/**
 * Priority of the CAN interrupts
 */
#define UNI_HAL_CAN_IRQ_PRIORITY (4U)

/**
 * Message RAM of one instance, in elements
 */
#define UNI_HAL_CAN_FILTERS      (14U)
#define UNI_HAL_CAN_RX_FIFO_SIZE (32U)
#define UNI_HAL_CAN_TX_BUFFERS   (3U)
#define UNI_HAL_CAN_RX_FIFOS     (2U)

/**
 * Size of the message RAM elements in 32-bit words: a filter for standard identifiers, one for
 * extended identifiers, and an RX or TX element with two header words and 8 data bytes
 */
#define UNI_HAL_CAN_STD_FILTER_WORDS (1U)
#define UNI_HAL_CAN_EXT_FILTER_WORDS (2U)
#define UNI_HAL_CAN_ELEMENT_WORDS    (4U)

/**
 * Where the sections of one instance start inside its block of the message RAM, in words
 */
#define UNI_HAL_CAN_RAM_STD_FILTERS (0U)
#define UNI_HAL_CAN_RAM_EXT_FILTERS (UNI_HAL_CAN_RAM_STD_FILTERS + UNI_HAL_CAN_FILTERS * UNI_HAL_CAN_STD_FILTER_WORDS)
#define UNI_HAL_CAN_RAM_RX_FIFO0    (UNI_HAL_CAN_RAM_EXT_FILTERS + UNI_HAL_CAN_FILTERS * UNI_HAL_CAN_EXT_FILTER_WORDS)
#define UNI_HAL_CAN_RAM_RX_FIFO1    (UNI_HAL_CAN_RAM_RX_FIFO0 + UNI_HAL_CAN_RX_FIFO_SIZE * UNI_HAL_CAN_ELEMENT_WORDS)
#define UNI_HAL_CAN_RAM_TX          (UNI_HAL_CAN_RAM_RX_FIFO1 + UNI_HAL_CAN_RX_FIFO_SIZE * UNI_HAL_CAN_ELEMENT_WORDS)
#define UNI_HAL_CAN_RAM_USED        (UNI_HAL_CAN_RAM_TX + UNI_HAL_CAN_TX_BUFFERS * UNI_HAL_CAN_ELEMENT_WORDS)

/**
 * The message RAM of 2560 words is shared by both instances; each gets a block of this size
 */
#define UNI_HAL_CAN_RAM_BLOCK_WORDS (320U)

_Static_assert(UNI_HAL_CAN_RAM_USED <= UNI_HAL_CAN_RAM_BLOCK_WORDS, "the message RAM sections do not fit their block");

/**
 * Bits of all TX buffers in the TXBxx registers
 */
#define UNI_HAL_CAN_TX_MASK ((1UL << UNI_HAL_CAN_TX_BUFFERS) - 1UL)

/**
 * Fields of the message RAM elements
 */
#define UNI_HAL_CAN_ELEMENT_XTD      (1UL << 30U) // extended identifier
#define UNI_HAL_CAN_ELEMENT_STD_POS  (18U)        // a standard identifier sits in bits 28:18
#define UNI_HAL_CAN_ELEMENT_DLC_POS  (16U)
#define UNI_HAL_CAN_FILTER_CLASSIC   (2UL << 30U) // identifier and mask
#define UNI_HAL_CAN_FILTER_STD_FIFO0 (1UL << 27U)
#define UNI_HAL_CAN_FILTER_STD_FIFO1 (2UL << 27U)
#define UNI_HAL_CAN_FILTER_EXT_FIFO0 (1UL << 29U)
#define UNI_HAL_CAN_FILTER_EXT_FIFO1 (2UL << 29U)

/**
 * Global filter: reject the frames that match no filter element
 */
#define UNI_HAL_CAN_GFC_REJECT (2UL)

/**
 * IDE bit in the bxCAN filter register layout that uni_hal_can_set_filter() takes
 */
#define UNI_HAL_CAN_BXCAN_IDE (1UL << 2U)

/**
 * Lower 18 bits of an extended identifier in the same layout (bits 20:3)
 */
#define UNI_HAL_CAN_BXCAN_EXID_LOW (0x001FFFF8UL)



//
// Context Storage
//

typedef struct {
    FDCAN_GlobalTypeDef *can;

    /** start of the block of this instance in the message RAM, in words */
    uint32_t ram_words;

    IRQn_Type irq;

    uni_hal_can_context_t *ctx;

    /** TX buffers that hold a frame whose completion has not been reported yet */
    volatile uint32_t tx_inflight;
} uni_hal_can_stm32h7_instance_t;

static uni_hal_can_stm32h7_instance_t g_uni_hal_can_instance[] = {
    {.can = FDCAN1, .ram_words = 0U, .irq = FDCAN1_IT0_IRQn},
    {.can = FDCAN2, .ram_words = UNI_HAL_CAN_RAM_BLOCK_WORDS, .irq = FDCAN2_IT0_IRQn},
};



//
// Private functions
//

static uni_hal_can_stm32h7_instance_t *_uni_hal_can_get_instance(uni_hal_core_periph_e instance) {
    uni_hal_can_stm32h7_instance_t *result = nullptr;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_CAN_1:
        result = &g_uni_hal_can_instance[0];
        break;
    case UNI_HAL_CORE_PERIPH_CAN_2:
        result = &g_uni_hal_can_instance[1];
        break;
    default:
        break;
    }

    return result;
}


/**
 * Get the address of a section of the message RAM of an instance
 * @param section one of UNI_HAL_CAN_RAM_xxx
 * @param index element number inside the section
 * @param element_words size of one element of the section
 */
static volatile uint32_t *_uni_hal_can_ram(const uni_hal_can_stm32h7_instance_t *inst, uint32_t section,
                                           uint32_t index, uint32_t element_words) {
    return (volatile uint32_t *)(SRAMCAN_BASE + 4U * (inst->ram_words + section + index * element_words));
}


/**
 * Wait for bits of a register
 * @param reg register to watch
 * @param mask bits to look at
 * @param set true to wait until all of them are set, false until all are clear
 * @return false when that did not happen within UNI_HAL_CAN_MODE_TIMEOUT_MS
 */
static bool _uni_hal_can_wait(const volatile uint32_t *reg, uint32_t mask, bool set) {
    uint32_t const start_ms = uni_hal_systick_get_ms();
    for (;;) {
        uint32_t const bits = *reg & mask;
        if (set ? (bits == mask) : (bits == 0U)) {
            return true;
        }
        if ((uni_hal_systick_get_ms() - start_ms) > UNI_HAL_CAN_MODE_TIMEOUT_MS) {
            return false;
        }
    }
}


/**
 * Take the peripheral off the bus and open its configuration for writing
 */
static bool _uni_hal_can_mode_init(FDCAN_GlobalTypeDef *can) {
    SET_BIT(can->CCCR, FDCAN_CCCR_INIT);
    bool const result = _uni_hal_can_wait(&can->CCCR, FDCAN_CCCR_INIT, true);
    if (result) {
        SET_BIT(can->CCCR, FDCAN_CCCR_CCE);
    }
    return result;
}


/**
 * Check that the peripheral takes part in bus traffic
 */
static bool _uni_hal_can_is_started(const FDCAN_GlobalTypeDef *can) {
    return (can->CCCR & FDCAN_CCCR_INIT) == 0U;
}


/**
 * Put a frame into a free TX buffer
 * @param ctx CAN context, must be initialised
 * @param msg frame to send
 * @param tx_buffer receives the number of the buffer the frame went into, 0..2
 * @return false when all buffers are taken or the peripheral is not started
 */
static bool _uni_hal_can_queue(uni_hal_can_context_t *ctx, const uni_hal_can_msg_t *msg, uint32_t *tx_buffer) {
    uni_hal_can_stm32h7_instance_t *inst = _uni_hal_can_get_instance(ctx->config.instance);
    if (inst == nullptr || !_uni_hal_can_is_started(inst->can)) {
        return false;
    }
    FDCAN_GlobalTypeDef *can = inst->can;

    uint32_t buffer = UNI_HAL_CAN_TX_BUFFERS;
    if (ctx->config.tx_fifo_priority) {
        // the buffers form a FIFO: the peripheral says where the next frame goes
        if ((can->TXFQS & FDCAN_TXFQS_TFQF) == 0U) {
            buffer = (can->TXFQS & FDCAN_TXFQS_TFQPI) >> FDCAN_TXFQS_TFQPI_Pos;
        }
    }
    else {
        // dedicated buffers: any that has no request pending
        uint32_t const pending = can->TXBRP;
        for (uint32_t idx = 0U; idx < UNI_HAL_CAN_TX_BUFFERS; idx++) {
            if ((pending & (1UL << idx)) == 0U) {
                buffer = idx;
                break;
            }
        }
    }
    if (buffer >= UNI_HAL_CAN_TX_BUFFERS) {
        return false;
    }

    uint32_t const dlc = (msg->dlc <= 8U) ? msg->dlc : 8U;

    volatile uint32_t *element = _uni_hal_can_ram(inst, UNI_HAL_CAN_RAM_TX, buffer, UNI_HAL_CAN_ELEMENT_WORDS);
    element[0] = msg->standard_id ? ((msg->id & 0x7FFU) << UNI_HAL_CAN_ELEMENT_STD_POS)
                                  : ((msg->id & 0x1FFFFFFFU) | UNI_HAL_CAN_ELEMENT_XTD);
    // classic frame, no bit rate switch, no TX event
    element[1] = dlc << UNI_HAL_CAN_ELEMENT_DLC_POS;
    element[2] = ((uint32_t)msg->data[3] << 24U) | ((uint32_t)msg->data[2] << 16U) |
                 ((uint32_t)msg->data[1] << 8U) | (uint32_t)msg->data[0];
    element[3] = ((uint32_t)msg->data[7] << 24U) | ((uint32_t)msg->data[6] << 16U) |
                 ((uint32_t)msg->data[5] << 8U) | (uint32_t)msg->data[4];

    // the interrupt clears the same bookkeeping, so the request goes out with it masked
    uint32_t const primask = uni_hal_core_irq_pause();
    ctx->status.tx_ok[buffer] = false;
    inst->tx_inflight |= 1UL << buffer;
    can->TXBAR = 1UL << buffer;
    uni_hal_core_irq_resume(primask);

    *tx_buffer = buffer;
    return true;
}



//
// Interrupts
//

/**
 * Record errors and tell the application
 * @param errors bits of uni_hal_can_error_e
 * @param counted true for what counts as a bus error in count_err; a frame dropped for lack
 *                of queue space has its own counter
 * @return not 0 when the callback made a task of higher priority ready
 */
static BaseType_t _uni_hal_can_report(uni_hal_can_context_t *ctx, uint32_t errors, bool counted) {
    BaseType_t woken = pdFALSE;

    if (ctx != nullptr && errors != UNI_HAL_CAN_ERROR_NONE) {
        if (counted) {
            ctx->status.count_err++;
        }
        ctx->status.errors |= errors;

        uni_hal_can_error_callback_t const callback = ctx->status.error_callback;
        if (callback != nullptr && callback(ctx->status.error_callback_cookie, errors)) {
            woken = pdTRUE;
        }
    }

    return woken;
}


/**
 * Move every frame of one RX FIFO to the receive queue
 * @return not 0 when a task of higher priority became ready
 */
static BaseType_t _uni_hal_can_irq_rx(const uni_hal_can_stm32h7_instance_t *inst, uint32_t fifo) {
    BaseType_t woken = pdFALSE;
    FDCAN_GlobalTypeDef *can = inst->can;
    uni_hal_can_context_t *ctx = inst->ctx;

    // the fields sit at the same positions in the registers of both FIFOs
    const volatile uint32_t *const status = (fifo == 0U) ? &can->RXF0S : &can->RXF1S;
    volatile uint32_t *const acknowledge = (fifo == 0U) ? &can->RXF0A : &can->RXF1A;
    uint32_t const section = (fifo == 0U) ? UNI_HAL_CAN_RAM_RX_FIFO0 : UNI_HAL_CAN_RAM_RX_FIFO1;

    while ((*status & FDCAN_RXF0S_F0FL) != 0U) {
        uint32_t const get_index = (*status & FDCAN_RXF0S_F0GI) >> FDCAN_RXF0S_F0GI_Pos;
        const volatile uint32_t *element = _uni_hal_can_ram(inst, section, get_index, UNI_HAL_CAN_ELEMENT_WORDS);

        uint32_t const r0 = element[0];
        uint32_t const r1 = element[1];
        uint32_t const data_low = element[2];
        uint32_t const data_high = element[3];

        // hand the element back to the FIFO
        *acknowledge = get_index;

        uni_hal_can_msg_t msg;
        msg.standard_id = (r0 & UNI_HAL_CAN_ELEMENT_XTD) == 0U;
        msg.id = msg.standard_id ? ((r0 >> UNI_HAL_CAN_ELEMENT_STD_POS) & 0x7FFU) : (r0 & 0x1FFFFFFFU);
        msg.dlc = (uint8_t)((r1 >> UNI_HAL_CAN_ELEMENT_DLC_POS) & 0xFU);
        if (msg.dlc > 8U) {
            msg.dlc = 8U;
        }
        msg.data[0] = (uint8_t)(data_low);
        msg.data[1] = (uint8_t)(data_low >> 8U);
        msg.data[2] = (uint8_t)(data_low >> 16U);
        msg.data[3] = (uint8_t)(data_low >> 24U);
        msg.data[4] = (uint8_t)(data_high);
        msg.data[5] = (uint8_t)(data_high >> 8U);
        msg.data[6] = (uint8_t)(data_high >> 16U);
        msg.data[7] = (uint8_t)(data_high >> 24U);

        if (ctx != nullptr && ctx->status.inited) {
#if defined(UNI_HAL_CAN_USE_FREERTOS)
            bool const queued = xQueueSendFromISR(ctx->status.queue_rx, &msg, &woken) == pdPASS;
#else
            bool const queued = uni_common_ringbuffer_push(ctx->config.buffer_rx, (uint8_t *)&msg, 1U) == 1U;
#endif

            if (queued) {
                ctx->status.count_rx++;
            }
            else {
                ctx->status.count_rx_dropped++;
                if (_uni_hal_can_report(ctx, UNI_HAL_CAN_ERROR_RX_DROPPED, false) != pdFALSE) {
                    woken = pdTRUE;
                }
            }
        }
    }

    return woken;
}


/**
 * Report the TX buffers that are done with their frame
 * @return not 0 when a task of higher priority became ready
 */
static BaseType_t _uni_hal_can_irq_tx(uni_hal_can_stm32h7_instance_t *inst) {
    BaseType_t woken = pdFALSE;
    FDCAN_GlobalTypeDef *can = inst->can;
    uni_hal_can_context_t *ctx = inst->ctx;

    // Done is every buffer that was given a frame and has no request pending any more. Take
    // them all off the books before any callback runs: a callback may queue the next frame.
    uint32_t const done = inst->tx_inflight & ~can->TXBRP & UNI_HAL_CAN_TX_MASK;
    uint32_t const transmitted = can->TXBTO;
    inst->tx_inflight &= ~done;

    for (uint32_t buffer = 0U; buffer < UNI_HAL_CAN_TX_BUFFERS; buffer++) {
        if ((done & (1UL << buffer)) != 0U) {
            // not transmitted means cancelled: by an abort, or after the single attempt that is
            // made with automatic retransmission off
            bool const success = (transmitted & (1UL << buffer)) != 0U;

            if (ctx != nullptr) {
                ctx->status.tx_ok[buffer] = success;

                uni_hal_can_tx_callback_t const callback = ctx->status.tx_callback;
                if (callback != nullptr && callback(ctx->status.tx_callback_cookie, buffer, success)) {
                    woken = pdTRUE;
                }
            }
        }
    }

    return woken;
}


/**
 * The error state of the node changed
 * @param flags the status change flags of the interrupt register that are set
 * @return not 0 when a task of higher priority became ready
 */
static BaseType_t _uni_hal_can_irq_error(const uni_hal_can_stm32h7_instance_t *inst, uint32_t flags) {
    FDCAN_GlobalTypeDef *can = inst->can;
    uni_hal_can_context_t *ctx = inst->ctx;

    // the flags tell that a state changed, the protocol status tells which way
    uint32_t const psr = can->PSR;

    uint32_t errors = UNI_HAL_CAN_ERROR_NONE;
    if ((flags & FDCAN_IR_EW) != 0U && (psr & FDCAN_PSR_EW) != 0U) {
        errors |= UNI_HAL_CAN_ERROR_WARNING;
    }
    if ((flags & FDCAN_IR_EP) != 0U && (psr & FDCAN_PSR_EP) != 0U) {
        errors |= UNI_HAL_CAN_ERROR_PASSIVE;
    }
    if ((flags & FDCAN_IR_BO) != 0U && (psr & FDCAN_PSR_BO) != 0U) {
        errors |= UNI_HAL_CAN_ERROR_BUS_OFF;

        // FDCAN stops in bus-off with INIT set and waits for the software. Clearing INIT starts
        // the recovery sequence of 129 x 11 recessive bits, which is what bxCAN does on its own
        // with ABOM set.
        if (ctx != nullptr && ctx->config.auto_bus_off) {
            CLEAR_BIT(can->CCCR, FDCAN_CCCR_INIT);
        }
    }

    return _uni_hal_can_report(ctx, errors, true);
}


static BaseType_t _uni_hal_can_irq(uni_hal_can_stm32h7_instance_t *inst) {
    BaseType_t woken = pdFALSE;
    FDCAN_GlobalTypeDef *can = inst->can;

    // acknowledge what is handled below; a flag that is set again meanwhile raises the
    // interrupt once more
    uint32_t const flags = can->IR & can->IE;
    can->IR = flags;

    if ((flags & (FDCAN_IR_RF0L | FDCAN_IR_RF1L)) != 0U) {
        if (_uni_hal_can_report(inst->ctx, UNI_HAL_CAN_ERROR_RX_OVERRUN, true) != pdFALSE) {
            woken = pdTRUE;
        }
    }
    if ((flags & FDCAN_IR_RF0N) != 0U && _uni_hal_can_irq_rx(inst, 0U) != pdFALSE) {
        woken = pdTRUE;
    }
    if ((flags & FDCAN_IR_RF1N) != 0U && _uni_hal_can_irq_rx(inst, 1U) != pdFALSE) {
        woken = pdTRUE;
    }
    if ((flags & (FDCAN_IR_TC | FDCAN_IR_TCF)) != 0U && _uni_hal_can_irq_tx(inst) != pdFALSE) {
        woken = pdTRUE;
    }
    if ((flags & (FDCAN_IR_EW | FDCAN_IR_EP | FDCAN_IR_BO)) != 0U && _uni_hal_can_irq_error(inst, flags) != pdFALSE) {
        woken = pdTRUE;
    }

    return woken;
}


void FDCAN1_IT0_IRQHandler(void)
{
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_can_irq(&g_uni_hal_can_instance[0]));
}

void FDCAN2_IT0_IRQHandler(void)
{
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_can_irq(&g_uni_hal_can_instance[1]));
}



//
// Functions
//

bool uni_hal_can_init(uni_hal_can_context_t *ctx) {
    bool result = false;
    uni_hal_can_stm32h7_instance_t *inst = (ctx != nullptr) ? _uni_hal_can_get_instance(ctx->config.instance) : nullptr;

    if (inst != nullptr) {
        FDCAN_GlobalTypeDef *can = inst->can;
        inst->ctx = ctx;
        inst->tx_inflight = 0U;
        result = true;

#if defined(UNI_HAL_CAN_USE_FREERTOS)
        ctx->status.queue_rx = xQueueCreate(UNI_HAL_CAN_QUEUE_SIZE, sizeof(uni_hal_can_msg_t));
        result = ctx->status.queue_rx != nullptr;
#else
        result = uni_common_ringbuffer_init(ctx->config.buffer_rx, ctx->config.buffer_rx->data, ctx->config.buffer_rx->size_object, ctx->config.buffer_rx->size_total);
#endif

        // both instances share one kernel clock; UNKNOWN keeps the source that is selected
        if (ctx->config.clock_source != UNI_HAL_RCC_CLKSRC_UNKNOWN) {
            result = result && uni_hal_rcc_clksrc_set(ctx->config.instance, ctx->config.clock_source);
        }
        result = result && uni_hal_rcc_clk_set(ctx->config.instance, true);
        result = result && uni_hal_gpio_pin_init(ctx->config.pin_rx);
        result = result && uni_hal_gpio_pin_init(ctx->config.pin_tx);

        // There is no timing to fall back to: the kernel clock is whatever the project selected.
        // The prescaler field is 9 bits wide.
        uni_hal_can_timing_t timing = {0};
        result = result && ctx->config.bitrate != 0U &&
                 uni_hal_can_timing_calc(uni_hal_rcc_clk_get_freq(ctx->config.instance), ctx->config.bitrate, &timing) &&
                 timing.prescaler <= 512U;

        if (result) {
            // wake the peripheral up, then open the configuration
            CLEAR_BIT(can->CCCR, FDCAN_CCCR_CSR);
            result = _uni_hal_can_wait(&can->CCCR, FDCAN_CCCR_CSA, false) && _uni_hal_can_mode_init(can);
        }

        if (result) {
            // classic CAN, normal operation; DAR disables the automatic retransmission
            MODIFY_REG(can->CCCR,
                       FDCAN_CCCR_DAR | FDCAN_CCCR_MON | FDCAN_CCCR_TEST | FDCAN_CCCR_ASM | FDCAN_CCCR_FDOE |
                       FDCAN_CCCR_BRSE | FDCAN_CCCR_TXP | FDCAN_CCCR_PXHD,
                       ctx->config.auto_retransmission ? 0U : FDCAN_CCCR_DAR);

            can->NBTP = ((timing.sjw - 1U) << FDCAN_NBTP_NSJW_Pos) | ((timing.prescaler - 1U) << FDCAN_NBTP_NBRP_Pos) |
                        ((timing.bs1 - 1U) << FDCAN_NBTP_NTSEG1_Pos) | ((timing.bs2 - 1U) << FDCAN_NBTP_NTSEG2_Pos);

            // message RAM: all filters start out disabled
            volatile uint32_t *ram = _uni_hal_can_ram(inst, 0U, 0U, 1U);
            for (uint32_t idx = 0U; idx < UNI_HAL_CAN_RAM_USED; idx++) {
                ram[idx] = 0U;
            }

            can->SIDFC = (UNI_HAL_CAN_FILTERS << FDCAN_SIDFC_LSS_Pos) |
                         ((inst->ram_words + UNI_HAL_CAN_RAM_STD_FILTERS) << FDCAN_SIDFC_FLSSA_Pos);
            can->XIDFC = (UNI_HAL_CAN_FILTERS << FDCAN_XIDFC_LSE_Pos) |
                         ((inst->ram_words + UNI_HAL_CAN_RAM_EXT_FILTERS) << FDCAN_XIDFC_FLESA_Pos);
            can->XIDAM = FDCAN_XIDAM_EIDM;
            can->GFC = (UNI_HAL_CAN_GFC_REJECT << FDCAN_GFC_ANFS_Pos) | (UNI_HAL_CAN_GFC_REJECT << FDCAN_GFC_ANFE_Pos);

            // a full FIFO keeps what it has and drops the new frame
            can->RXF0C = (UNI_HAL_CAN_RX_FIFO_SIZE << FDCAN_RXF0C_F0S_Pos) |
                         ((inst->ram_words + UNI_HAL_CAN_RAM_RX_FIFO0) << FDCAN_RXF0C_F0SA_Pos);
            can->RXF1C = (UNI_HAL_CAN_RX_FIFO_SIZE << FDCAN_RXF1C_F1S_Pos) |
                         ((inst->ram_words + UNI_HAL_CAN_RAM_RX_FIFO1) << FDCAN_RXF1C_F1SA_Pos);
            can->RXBC = 0U;
            // 8 data bytes per element, RX and TX
            can->RXESC = 0U;
            can->TXESC = 0U;
            can->TXEFC = 0U;

            // Three TX buffers. As dedicated buffers the frame with the lowest identifier goes
            // first, as a FIFO they go in the order they were requested.
            uint32_t const tx_base = (inst->ram_words + UNI_HAL_CAN_RAM_TX) << FDCAN_TXBC_TBSA_Pos;
            can->TXBC = ctx->config.tx_fifo_priority ? (tx_base | (UNI_HAL_CAN_TX_BUFFERS << FDCAN_TXBC_TFQS_Pos))
                                                    : (tx_base | (UNI_HAL_CAN_TX_BUFFERS << FDCAN_TXBC_NDTB_Pos));

            // Received frames, lost frames and changes of the error state, all on interrupt
            // line 0. The transmit interrupts are enabled with the transmit callback.
            can->IR = 0xFFFFFFFFU;
            can->TXBTIE = 0U;
            can->TXBCIE = 0U;
            can->IE = FDCAN_IE_RF0NE | FDCAN_IE_RF0LE | FDCAN_IE_RF1NE | FDCAN_IE_RF1LE | FDCAN_IE_EWE | FDCAN_IE_EPE |
                      FDCAN_IE_BOE;
            can->ILS = 0U;
            can->ILE = FDCAN_ILE_EINT0;

            NVIC_SetPriority(inst->irq, UNI_HAL_CAN_IRQ_PRIORITY);
            NVIC_EnableIRQ(inst->irq);
        }

        ctx->status.count_rx = 0U;
        ctx->status.count_tx = 0U;
        ctx->status.count_err = 0U;
        ctx->status.count_rx_dropped = 0U;
        ctx->status.errors = UNI_HAL_CAN_ERROR_NONE;
        ctx->status.tx_callback = nullptr;
        ctx->status.tx_callback_cookie = nullptr;
        ctx->status.error_callback = nullptr;
        ctx->status.error_callback_cookie = nullptr;
        for (uint32_t buffer = 0U; buffer < UNI_HAL_CAN_TX_BUFFERS; buffer++) {
            ctx->status.tx_ok[buffer] = false;
        }
        ctx->status.inited = result;
    }

    return result;
}

bool uni_hal_can_start(uni_hal_can_context_t *ctx) {
    bool result = false;
    if (uni_hal_can_is_inited(ctx)) {
        FDCAN_GlobalTypeDef *can = _uni_hal_can_get_instance(ctx->config.instance)->can;

        // leave initialisation mode, then wait for the peripheral to have joined the bus: its
        // activity is 'synchronizing' (0) until it has seen 11 recessive bits
        CLEAR_BIT(can->CCCR, FDCAN_CCCR_INIT);
        result = _uni_hal_can_wait(&can->CCCR, FDCAN_CCCR_INIT, false);
        if (result) {
            uint32_t const start_ms = uni_hal_systick_get_ms();
            while ((can->PSR & FDCAN_PSR_ACT) == 0U) {
                if ((uni_hal_systick_get_ms() - start_ms) > UNI_HAL_CAN_MODE_TIMEOUT_MS) {
                    result = false;
                    break;
                }
            }
        }

        if (!result) {
            // The bus was not idle in time (e.g. stuck dominant). Go back to initialisation
            // mode, so that the peripheral is in a defined state and the start can be retried.
            (void)_uni_hal_can_mode_init(can);
        }
    }

    return result;
}

bool uni_hal_can_stop(uni_hal_can_context_t *ctx) {
    bool result = false;
    if (uni_hal_can_is_inited(ctx)) {
        result = _uni_hal_can_mode_init(_uni_hal_can_get_instance(ctx->config.instance)->can);
    }

    return result;
}

bool uni_hal_can_set_filter(uni_hal_can_context_t *ctx, uint32_t fifo_num, uint32_t slot_idx, uint32_t filter_id,
                           uint32_t filter_mask) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx) && fifo_num < UNI_HAL_CAN_RX_FIFOS) {
        const uni_hal_can_stm32h7_instance_t *inst = _uni_hal_can_get_instance(ctx->config.instance);

        // bxCAN numbers the banks of its second instance from 14; accept those numbers here
        // too, so that the same call works on both MCU families
        if (ctx->config.instance == UNI_HAL_CORE_PERIPH_CAN_2 && slot_idx >= UNI_HAL_CAN_FILTERS) {
            slot_idx -= UNI_HAL_CAN_FILTERS;
        }

        if (slot_idx < UNI_HAL_CAN_FILTERS) {
            // The identifier and the mask come in the layout of the bxCAN filter registers:
            // a standard identifier in bits 31:21, an extended one in bits 31:3, IDE in bit 2.
            // FDCAN keeps separate filter lists for the two kinds. A mask that does not look at
            // IDE lets both kinds through on bxCAN, so both lists get an element then. The RTR
            // bit of the bxCAN layout has no counterpart in an FDCAN filter and is ignored.
            bool const ide_matters = (filter_mask & UNI_HAL_CAN_BXCAN_IDE) != 0U;
            bool const ide_extended = (filter_id & UNI_HAL_CAN_BXCAN_IDE) != 0U;
            // In that layout a standard frame has zeros where an extended identifier has its
            // lower 18 bits, so it only matches when the filter asks for nothing else there.
            bool const low_bits_clear = (filter_id & filter_mask & UNI_HAL_CAN_BXCAN_EXID_LOW) == 0U;
            bool const want_standard = (!ide_matters || !ide_extended) && low_bits_clear;
            bool const want_extended = !ide_matters || ide_extended;

            volatile uint32_t *std_element =
                    _uni_hal_can_ram(inst, UNI_HAL_CAN_RAM_STD_FILTERS, slot_idx, UNI_HAL_CAN_STD_FILTER_WORDS);
            volatile uint32_t *ext_element =
                    _uni_hal_can_ram(inst, UNI_HAL_CAN_RAM_EXT_FILTERS, slot_idx, UNI_HAL_CAN_EXT_FILTER_WORDS);

            // an element with its configuration field at 0 is disabled: take both out of use
            // while they change
            std_element[0] = 0U;
            ext_element[0] = 0U;

            if (want_standard) {
                std_element[0] = UNI_HAL_CAN_FILTER_CLASSIC |
                                 ((fifo_num == 1U) ? UNI_HAL_CAN_FILTER_STD_FIFO1 : UNI_HAL_CAN_FILTER_STD_FIFO0) |
                                 (((filter_id >> 21U) & 0x7FFU) << 16U) | ((filter_mask >> 21U) & 0x7FFU);
            }
            if (want_extended) {
                ext_element[1] = UNI_HAL_CAN_FILTER_CLASSIC | ((filter_mask >> 3U) & 0x1FFFFFFFU);
                ext_element[0] = ((fifo_num == 1U) ? UNI_HAL_CAN_FILTER_EXT_FIFO1 : UNI_HAL_CAN_FILTER_EXT_FIFO0) |
                                 ((filter_id >> 3U) & 0x1FFFFFFFU);
            }

            result = true;
        }
    }

    return result;
}


uint32_t uni_hal_can_transmit_free(const uni_hal_can_context_t *ctx) {
    uint32_t result = 0U;
    if (uni_hal_can_is_inited(ctx)) {
        const FDCAN_GlobalTypeDef *can = _uni_hal_can_get_instance(ctx->config.instance)->can;
        if (ctx->config.tx_fifo_priority) {
            result = (can->TXFQS & FDCAN_TXFQS_TFFL) >> FDCAN_TXFQS_TFFL_Pos;
        }
        else {
            uint32_t const pending = can->TXBRP;
            for (uint32_t buffer = 0U; buffer < UNI_HAL_CAN_TX_BUFFERS; buffer++) {
                result += ((pending & (1UL << buffer)) == 0U) ? 1U : 0U;
            }
        }
    }
    return result;
}


bool uni_hal_can_transmit_nowait(uni_hal_can_context_t *ctx, const uni_hal_can_msg_t *msg) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx) && msg != nullptr) {
        uint32_t tx_buffer = 0U;
        result = _uni_hal_can_queue(ctx, msg, &tx_buffer);
        if (result) {
            // counted when queued: the outcome of the frame is not followed up here
            ctx->status.count_tx++;
        }
    }

    return result;
}


bool uni_hal_can_set_tx_callback(uni_hal_can_context_t *ctx, uni_hal_can_tx_callback_t callback, void *cookie) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx)) {
        uni_hal_can_stm32h7_instance_t *inst = _uni_hal_can_get_instance(ctx->config.instance);
        FDCAN_GlobalTypeDef *can = inst->can;

        // the transmit interrupts are only needed while somebody listens; switch them off first
        // so that they never see a half-updated callback
        CLEAR_BIT(can->IE, FDCAN_IE_TCE | FDCAN_IE_TCFE);

        ctx->status.tx_callback_cookie = cookie;
        ctx->status.tx_callback = callback;

        if (callback != nullptr) {
            // frames that completed before the callback existed are not reported: only what is
            // still on its way counts
            uint32_t const primask = uni_hal_core_irq_pause();
            inst->tx_inflight = can->TXBRP & UNI_HAL_CAN_TX_MASK;
            uni_hal_core_irq_resume(primask);

            can->IR = FDCAN_IR_TC | FDCAN_IR_TCF;
            can->TXBTIE = UNI_HAL_CAN_TX_MASK;
            can->TXBCIE = UNI_HAL_CAN_TX_MASK;
            SET_BIT(can->IE, FDCAN_IE_TCE | FDCAN_IE_TCFE);
        }
        else {
            can->TXBTIE = 0U;
            can->TXBCIE = 0U;
        }

        result = true;
    }

    return result;
}


bool uni_hal_can_transmit_abort(uni_hal_can_context_t *ctx) {
    bool result = false;
    if (uni_hal_can_is_inited(ctx)) {
        _uni_hal_can_get_instance(ctx->config.instance)->can->TXBCR = UNI_HAL_CAN_TX_MASK;
        result = true;
    }
    return result;
}


bool uni_hal_can_transmit(uni_hal_can_context_t *ctx, uni_hal_can_msg_t *msg) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx) && msg != nullptr) {
        uint32_t tx_buffer = 0U;

        if (_uni_hal_can_queue(ctx, msg, &tx_buffer)) {
            FDCAN_GlobalTypeDef *can = _uni_hal_can_get_instance(ctx->config.instance)->can;
            uint32_t const buffer_bit = 1UL << tx_buffer;

            // wait until the buffer is done with the frame; in bus-off it never is
            uint32_t const start_ms = uni_hal_systick_get_ms();
            bool timed_out = false;
            while ((can->TXBRP & buffer_bit) != 0U) {
                if ((uni_hal_systick_get_ms() - start_ms) > UNI_HAL_CAN_TX_TIMEOUT_MS) {
                    can->TXBCR = buffer_bit;
                    timed_out = true;
                    break;
                }
            }

            // the request also ends when the frame was cancelled: 'transmission occurred' tells
            // whether it went out, and stays set until the buffer gets its next frame
            result = !timed_out && (can->TXBTO & buffer_bit) != 0U;
        }

        if (result) {
            ctx->status.count_tx++;
        }
        else {
            ctx->status.count_err++;
        }
    }

    return result;
}

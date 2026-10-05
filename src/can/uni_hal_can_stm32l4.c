//
// Includes
//

// stdlib
#include <stddef.h>

// st
#include <stm32l4xx.h>

// uni_hal
#include "can/uni_hal_can.h"
#include "os/uni_hal_os.h"
#include "rcc/uni_hal_rcc.h"
#include "systick/uni_hal_systick.h"



//
// The bxCAN peripheral is driven through its registers: ST ships no LL driver for it, and the
// HAL driver brings its own state machine and tick dependency that this driver does not need.
// Register and bit names are those of RM0351, chapter "Controller area network (bxCAN)".
//



//
// Defines
//

/**
 * Longest wait for one frame to leave its TX mailbox
 */
#define UNI_HAL_CAN_TX_TIMEOUT_MS (100U)

/**
 * Longest wait for the peripheral to enter or leave initialisation mode.
 * Leaving it needs 11 consecutive recessive bits on the bus.
 */
#define UNI_HAL_CAN_MODE_TIMEOUT_MS (10U)

/**
 * Priority of the CAN interrupts
 */
#define UNI_HAL_CAN_IRQ_PRIORITY (4U)

/**
 * Filter banks 0..13 belong to CAN1, 14..27 to CAN2
 */
#define UNI_HAL_CAN_FILTER_BANKS      (28U)
#define UNI_HAL_CAN_FILTER_CAN2_START (14U)

/**
 * Number of TX mailboxes and RX FIFOs of one instance
 */
#define UNI_HAL_CAN_TX_MAILBOXES (3U)
#define UNI_HAL_CAN_RX_FIFOS     (2U)

/**
 * A FIFO releases its output mailbox within a few clock cycles; do not spin on it for ever
 */
#define UNI_HAL_CAN_RELEASE_SPINS (1000U)



//
// Context Storage
//

static uni_hal_can_context_t *_uni_hal_can_1_ctx = nullptr;
static uni_hal_can_context_t *_uni_hal_can_2_ctx = nullptr;



//
// Private functions
//

static CAN_TypeDef *_uni_hal_can_get_handle(uni_hal_core_periph_e instance) {
    CAN_TypeDef *result = nullptr;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_CAN_1:
        result = CAN1;
        break;
    case UNI_HAL_CORE_PERIPH_CAN_2:
        result = CAN2;
        break;
    default:
        break;
    }

    return result;
}


static bool _uni_hal_can_set_context(uni_hal_can_context_t *ctx)
{
    bool result = false;
    switch (ctx->config.instance) {
    case UNI_HAL_CORE_PERIPH_CAN_1:
        _uni_hal_can_1_ctx = ctx;
        result = true;
        break;
    case UNI_HAL_CORE_PERIPH_CAN_2:
        _uni_hal_can_2_ctx = ctx;
        result = true;
        break;
    default:
        break;
    }

    return result;
}


/**
 * Enable CAN interrupts
 * @param instance target CAN instance
 * @param priority interrupt priority
 */
static bool _uni_hal_can_interrupt_enable(uni_hal_core_periph_e instance, uint32_t priority) {
    bool result = false;
    switch (instance) {
    case UNI_HAL_CORE_PERIPH_CAN_1:
        NVIC_SetPriority(CAN1_RX0_IRQn, priority);
        NVIC_EnableIRQ(CAN1_RX0_IRQn);

        NVIC_SetPriority(CAN1_RX1_IRQn, priority);
        NVIC_EnableIRQ(CAN1_RX1_IRQn);

        NVIC_SetPriority(CAN1_SCE_IRQn, priority);
        NVIC_EnableIRQ(CAN1_SCE_IRQn);

        NVIC_SetPriority(CAN1_TX_IRQn, priority);
        NVIC_EnableIRQ(CAN1_TX_IRQn);
        result = true;
        break;
    case UNI_HAL_CORE_PERIPH_CAN_2:
        NVIC_SetPriority(CAN2_RX0_IRQn, priority);
        NVIC_EnableIRQ(CAN2_RX0_IRQn);

        NVIC_SetPriority(CAN2_RX1_IRQn, priority);
        NVIC_EnableIRQ(CAN2_RX1_IRQn);

        NVIC_SetPriority(CAN2_SCE_IRQn, priority);
        NVIC_EnableIRQ(CAN2_SCE_IRQn);

        NVIC_SetPriority(CAN2_TX_IRQn, priority);
        NVIC_EnableIRQ(CAN2_TX_IRQn);
        result = true;
        break;
    default:
        break;
    }
    return result;
}


/**
 * Wait for bits of the master status register
 * @param can CAN instance
 * @param mask bits of CAN_MSR to look at
 * @param set true to wait until all of them are set, false until all are clear
 * @return false when that did not happen within UNI_HAL_CAN_MODE_TIMEOUT_MS
 */
static bool _uni_hal_can_wait_msr(const CAN_TypeDef *can, uint32_t mask, bool set) {
    uint32_t const start_ms = uni_hal_systick_get_ms();
    for (;;) {
        uint32_t const bits = can->MSR & mask;
        if (set ? (bits == mask) : (bits == 0U)) {
            return true;
        }
        if ((uni_hal_systick_get_ms() - start_ms) > UNI_HAL_CAN_MODE_TIMEOUT_MS) {
            return false;
        }
    }
}


/**
 * Put the peripheral into initialisation mode, where it takes no part in bus traffic
 */
static bool _uni_hal_can_mode_init(CAN_TypeDef *can) {
    SET_BIT(can->MCR, CAN_MCR_INRQ);
    return _uni_hal_can_wait_msr(can, CAN_MSR_INAK, true);
}


/**
 * Check that the peripheral takes part in bus traffic
 */
static bool _uni_hal_can_is_started(const CAN_TypeDef *can) {
    return (can->MSR & CAN_MSR_INAK) == 0U;
}


/**
 * Put a frame into a free TX mailbox
 * @param ctx CAN context, must be initialised
 * @param msg frame to send
 * @param tx_mailbox receives the number of the mailbox the frame went into, 0..2
 * @return false when all mailboxes are taken or the peripheral is not started
 */
static bool _uni_hal_can_queue(uni_hal_can_context_t *ctx, const uni_hal_can_msg_t *msg, uint32_t *tx_mailbox) {
    CAN_TypeDef *can = _uni_hal_can_get_handle(ctx->config.instance);
    if (can == nullptr || !_uni_hal_can_is_started(can)) {
        return false;
    }

    uint32_t const tsr = can->TSR;
    if ((tsr & (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2)) == 0U) {
        return false;
    }

    // CODE holds the number of the next free mailbox while at least one is free
    uint32_t const mailbox = (tsr & CAN_TSR_CODE) >> CAN_TSR_CODE_Pos;
    if (mailbox >= UNI_HAL_CAN_TX_MAILBOXES) {
        return false;
    }

    CAN_TxMailBox_TypeDef *box = &can->sTxMailBox[mailbox];
    uint32_t const dlc = (msg->dlc <= 8U) ? msg->dlc : 8U;

    // the outcome kept by the transmit interrupt belongs to the frame before this one
    ctx->status.tx_ok[mailbox] = false;

    // identifier, data frame; TXRQ is set last, once the rest of the mailbox is filled
    box->TIR = msg->standard_id ? ((msg->id & 0x7FFU) << CAN_TI0R_STID_Pos)
                                : (((msg->id & 0x1FFFFFFFU) << CAN_TI0R_EXID_Pos) | CAN_TI0R_IDE);
    box->TDTR = dlc;
    box->TDLR = ((uint32_t)msg->data[3] << 24U) | ((uint32_t)msg->data[2] << 16U) |
                ((uint32_t)msg->data[1] << 8U) | (uint32_t)msg->data[0];
    box->TDHR = ((uint32_t)msg->data[7] << 24U) | ((uint32_t)msg->data[6] << 16U) |
                ((uint32_t)msg->data[5] << 8U) | (uint32_t)msg->data[4];
    SET_BIT(box->TIR, CAN_TI0R_TXRQ);

    *tx_mailbox = mailbox;
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
 * Receive interrupt of one FIFO: move every pending frame to the receive queue
 * @return not 0 when a task of higher priority became ready
 */
static BaseType_t _uni_hal_can_irq_rx(uni_hal_can_context_t *ctx, CAN_TypeDef *can, uint32_t fifo) {
    BaseType_t woken = pdFALSE;

    // the status and control bits sit at the same positions in RF0R and RF1R
    volatile uint32_t *const rfr = (fifo == 0U) ? &can->RF0R : &can->RF1R;
    const CAN_FIFOMailBox_TypeDef *const box = &can->sFIFOMailBox[fifo];

    // a frame was lost because the FIFO was full
    if ((*rfr & CAN_RF0R_FOVR0) != 0U) {
        // the flags are cleared by writing 1; writing only this one leaves the others alone
        *rfr = CAN_RF0R_FOVR0;
        if (_uni_hal_can_report(ctx, UNI_HAL_CAN_ERROR_RX_OVERRUN, true) != pdFALSE) {
            woken = pdTRUE;
        }
    }

    while ((*rfr & CAN_RF0R_FMP0) != 0U) {
        uni_hal_can_msg_t msg;

        uint32_t const rir = box->RIR;
        uint32_t const rdtr = box->RDTR;
        uint32_t const rdlr = box->RDLR;
        uint32_t const rdhr = box->RDHR;

        msg.standard_id = (rir & CAN_RI0R_IDE) == 0U;
        msg.id = msg.standard_id ? (rir >> CAN_RI0R_STID_Pos) : (rir >> CAN_RI0R_EXID_Pos);
        msg.dlc = (uint8_t)(rdtr & CAN_RDT0R_DLC);
        if (msg.dlc > 8U) {
            msg.dlc = 8U;
        }
        msg.data[0] = (uint8_t)(rdlr);
        msg.data[1] = (uint8_t)(rdlr >> 8U);
        msg.data[2] = (uint8_t)(rdlr >> 16U);
        msg.data[3] = (uint8_t)(rdlr >> 24U);
        msg.data[4] = (uint8_t)(rdhr);
        msg.data[5] = (uint8_t)(rdhr >> 8U);
        msg.data[6] = (uint8_t)(rdhr >> 16U);
        msg.data[7] = (uint8_t)(rdhr >> 24U);

        // release the output mailbox and let the FIFO move on to the next frame
        *rfr = CAN_RF0R_RFOM0;
        for (uint32_t spin = 0U; spin < UNI_HAL_CAN_RELEASE_SPINS && (*rfr & CAN_RF0R_RFOM0) != 0U; spin++) {
        }

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
 * Transmit interrupt: one or more TX mailboxes are done with their frame
 * @return not 0 when a task of higher priority became ready
 */
static BaseType_t _uni_hal_can_irq_tx(uni_hal_can_context_t *ctx, CAN_TypeDef *can) {
    BaseType_t woken = pdFALSE;

    // Take the completed requests and acknowledge all of them before any callback runs: a
    // callback may queue the next frame, and a new request on a mailbox clears its completion
    // flag, which would otherwise be lost for a mailbox that is still waiting its turn here.
    uint32_t const tsr = can->TSR;
    uint32_t const done = tsr & (CAN_TSR_RQCP0 | CAN_TSR_RQCP1 | CAN_TSR_RQCP2);
    // RQCP is cleared by writing 1, which also clears TXOK, ALST and TERR of that mailbox
    can->TSR = done;

    for (uint32_t mailbox = 0U; mailbox < UNI_HAL_CAN_TX_MAILBOXES; mailbox++) {
        if ((done & (CAN_TSR_RQCP0 << (8U * mailbox))) != 0U) {
            bool const success = (tsr & (CAN_TSR_TXOK0 << (8U * mailbox))) != 0U;

            if (ctx != nullptr) {
                ctx->status.tx_ok[mailbox] = success;

                uni_hal_can_tx_callback_t const callback = ctx->status.tx_callback;
                if (callback != nullptr && callback(ctx->status.tx_callback_cookie, mailbox, success)) {
                    woken = pdTRUE;
                }
            }
        }
    }

    return woken;
}


/**
 * Status change interrupt: the error state of the node changed
 */
static BaseType_t _uni_hal_can_irq_sce(uni_hal_can_context_t *ctx, CAN_TypeDef *can) {
    BaseType_t woken = pdFALSE;

    if ((can->MSR & CAN_MSR_ERRI) != 0U) {
        uint32_t const esr = can->ESR;

        // cleared by writing 1; the other write-1-to-clear bits of MSR are written as 0
        can->MSR = CAN_MSR_ERRI;

        uint32_t errors = UNI_HAL_CAN_ERROR_NONE;
        if ((esr & CAN_ESR_EWGF) != 0U) {
            errors |= UNI_HAL_CAN_ERROR_WARNING;
        }
        if ((esr & CAN_ESR_EPVF) != 0U) {
            errors |= UNI_HAL_CAN_ERROR_PASSIVE;
        }
        if ((esr & CAN_ESR_BOFF) != 0U) {
            errors |= UNI_HAL_CAN_ERROR_BUS_OFF;
        }

        if (ctx != nullptr) {
            if (errors != UNI_HAL_CAN_ERROR_NONE) {
                woken = _uni_hal_can_report(ctx, errors, true);
            }
            else {
                // the interrupt came for a state the node has left again already
                ctx->status.count_err++;
            }
        }
    }

    return woken;
}


void CAN1_RX0_IRQHandler(void)
{
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_can_irq_rx(_uni_hal_can_1_ctx, CAN1, 0U));
}

void CAN1_RX1_IRQHandler(void)
{
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_can_irq_rx(_uni_hal_can_1_ctx, CAN1, 1U));
}

void CAN2_RX0_IRQHandler(void)
{
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_can_irq_rx(_uni_hal_can_2_ctx, CAN2, 0U));
}

void CAN2_RX1_IRQHandler(void)
{
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_can_irq_rx(_uni_hal_can_2_ctx, CAN2, 1U));
}

void CAN1_TX_IRQHandler(void)
{
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_can_irq_tx(_uni_hal_can_1_ctx, CAN1));
}

void CAN2_TX_IRQHandler(void)
{
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_can_irq_tx(_uni_hal_can_2_ctx, CAN2));
}

// status change / error
void CAN1_SCE_IRQHandler(void)
{
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_can_irq_sce(_uni_hal_can_1_ctx, CAN1));
}

void CAN2_SCE_IRQHandler(void)
{
    UNI_HAL_OS_ISR_ENTER();
    UNI_HAL_OS_ISR_EXIT(_uni_hal_can_irq_sce(_uni_hal_can_2_ctx, CAN2));
}



//
// Functions
//

bool uni_hal_can_init(uni_hal_can_context_t *ctx) {
    bool result = false;
    if (ctx != nullptr) {
        result = _uni_hal_can_set_context(ctx);

#if defined(UNI_HAL_CAN_USE_FREERTOS)
        ctx->status.queue_rx = xQueueCreate(UNI_HAL_CAN_QUEUE_SIZE, sizeof(uni_hal_can_msg_t));
        result = result && ctx->status.queue_rx != nullptr;
#else
        result =
            result && uni_common_ringbuffer_init(ctx->config.buffer_rx, ctx->config.buffer_rx->data, ctx->config.buffer_rx->size_object, ctx->config.buffer_rx->size_total);
#endif

        result = result && uni_hal_rcc_clk_set(ctx->config.instance, true);

        // CAN2 is the slave of CAN1: its acceptance filters live in the CAN1 registers
        if (ctx->config.instance == UNI_HAL_CORE_PERIPH_CAN_2) {
            result = result && uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_CAN_1, true);
        }
        result = result && uni_hal_gpio_pin_init(ctx->config.pin_rx);
        result = result && uni_hal_gpio_pin_init(ctx->config.pin_tx);

        CAN_TypeDef *can = _uni_hal_can_get_handle(ctx->config.instance);
        if (result && can != nullptr) {
            // the former fixed timing, used when no bit rate is configured
            uni_hal_can_timing_t timing = {.prescaler = 10U, .bs1 = 8U, .bs2 = 1U, .sjw = 1U};
            if (ctx->config.bitrate != 0U) {
                result = uni_hal_can_timing_calc(uni_hal_rcc_clk_get_freq(ctx->config.instance), ctx->config.bitrate,
                                                 &timing);
            }

            // the configuration below can only be written in initialisation mode; the peripheral
            // comes out of reset asleep, which has to be left as well
            result = result && _uni_hal_can_mode_init(can);
            if (result) {
                CLEAR_BIT(can->MCR, CAN_MCR_SLEEP);
                result = _uni_hal_can_wait_msr(can, CAN_MSR_SLAK, false);
            }

            if (result) {
                // bus management
                uint32_t mcr = 0U;
                if (ctx->config.auto_bus_off) {
                    mcr |= CAN_MCR_ABOM;
                }
                if (ctx->config.auto_wake_up) {
                    mcr |= CAN_MCR_AWUM;
                }
                if (!ctx->config.auto_retransmission) {
                    mcr |= CAN_MCR_NART;
                }
                if (ctx->config.tx_fifo_priority) {
                    mcr |= CAN_MCR_TXFP;
                }
                MODIFY_REG(can->MCR,
                           CAN_MCR_TTCM | CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_NART | CAN_MCR_RFLM | CAN_MCR_TXFP, mcr);

                // bit timing, normal mode (neither loop back nor silent)
                can->BTR = ((timing.sjw - 1U) << CAN_BTR_SJW_Pos) | ((timing.bs1 - 1U) << CAN_BTR_TS1_Pos) |
                           ((timing.bs2 - 1U) << CAN_BTR_TS2_Pos) | (timing.prescaler - 1U);

                // Report the changes of the error state and lost frames. The per-frame error code
                // interrupt (LEC) is left off: on a bus without a partner it would fire for every
                // retransmission. The receive interrupts are enabled with the first filter.
                can->IER = CAN_IER_ERRIE | CAN_IER_EWGIE | CAN_IER_EPVIE | CAN_IER_BOFIE | CAN_IER_FOVIE0 |
                           CAN_IER_FOVIE1;

                result = _uni_hal_can_interrupt_enable(ctx->config.instance, UNI_HAL_CAN_IRQ_PRIORITY);
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
            for (uint32_t mailbox = 0U; mailbox < UNI_HAL_CAN_TX_MAILBOXES; mailbox++) {
                ctx->status.tx_ok[mailbox] = false;
            }
            ctx->status.inited = result;
        }
    }

    return result;
}

bool uni_hal_can_start(uni_hal_can_context_t *ctx) { //-V2009
    bool result = false;
    if (uni_hal_can_is_inited(ctx)) {
        CAN_TypeDef *can = _uni_hal_can_get_handle(ctx->config.instance);

        // leave initialisation mode: the peripheral synchronises to the bus first
        CLEAR_BIT(can->MCR, CAN_MCR_INRQ);
        result = _uni_hal_can_wait_msr(can, CAN_MSR_INAK, false);
        if (!result) {
            // The bus was not idle in time (e.g. stuck dominant). Withdraw the request, so that
            // the peripheral is in a defined state and the start can be tried again later.
            (void)_uni_hal_can_mode_init(can);
        }
    }

    return result;
}

bool uni_hal_can_stop(uni_hal_can_context_t *ctx) { //-V2009
    bool result = false;
    if (uni_hal_can_is_inited(ctx)) {
        result = _uni_hal_can_mode_init(_uni_hal_can_get_handle(ctx->config.instance));
    }

    return result;
}

bool uni_hal_can_set_filter(uni_hal_can_context_t *ctx, uint32_t fifo_num, uint32_t slot_idx, uint32_t filter_id, //-V2009
                           uint32_t filter_mask) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx) && fifo_num < UNI_HAL_CAN_RX_FIFOS && slot_idx < UNI_HAL_CAN_FILTER_BANKS)
    {
        // a bank only filters for the instance it belongs to
        bool const is_can2 = ctx->config.instance == UNI_HAL_CORE_PERIPH_CAN_2;
        if (is_can2 == (slot_idx >= UNI_HAL_CAN_FILTER_CAN2_START)) {
            CAN_TypeDef *can = _uni_hal_can_get_handle(ctx->config.instance);
            uint32_t const bank_bit = 1UL << slot_idx;

            // the filter registers of both instances are part of CAN1
            SET_BIT(CAN1->FMR, CAN_FMR_FINIT);
            MODIFY_REG(CAN1->FMR, CAN_FMR_CAN2SB, UNI_HAL_CAN_FILTER_CAN2_START << CAN_FMR_CAN2SB_Pos);

            // a bank is set up while it is inactive
            CLEAR_BIT(CAN1->FA1R, bank_bit);

            // one 32-bit identifier with its mask, in the layout of the receive identifier
            // register: identifier shifted left, then IDE and RTR
            SET_BIT(CAN1->FS1R, bank_bit);
            CLEAR_BIT(CAN1->FM1R, bank_bit);
            CAN1->sFilterRegister[slot_idx].FR1 = filter_id;
            CAN1->sFilterRegister[slot_idx].FR2 = filter_mask;

            if (fifo_num == 1U) {
                SET_BIT(CAN1->FFA1R, bank_bit);
            }
            else {
                CLEAR_BIT(CAN1->FFA1R, bank_bit);
            }

            SET_BIT(CAN1->FA1R, bank_bit);
            CLEAR_BIT(CAN1->FMR, CAN_FMR_FINIT);

            // frames can arrive in this FIFO from now on
            SET_BIT(can->IER, (fifo_num == 1U) ? CAN_IER_FMPIE1 : CAN_IER_FMPIE0);

            result = true;
        }
    }

    return result;
}


uint32_t uni_hal_can_transmit_free(const uni_hal_can_context_t *ctx) {
    uint32_t result = 0U;
    if (uni_hal_can_is_inited(ctx)) {
        uint32_t const tsr = _uni_hal_can_get_handle(ctx->config.instance)->TSR;
        result += ((tsr & CAN_TSR_TME0) != 0U) ? 1U : 0U;
        result += ((tsr & CAN_TSR_TME1) != 0U) ? 1U : 0U;
        result += ((tsr & CAN_TSR_TME2) != 0U) ? 1U : 0U;
    }
    return result;
}


bool uni_hal_can_transmit_nowait(uni_hal_can_context_t *ctx, const uni_hal_can_msg_t *msg) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx) && msg != nullptr) {
        uint32_t tx_mailbox = 0U;
        result = _uni_hal_can_queue(ctx, msg, &tx_mailbox);
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
        CAN_TypeDef *can = _uni_hal_can_get_handle(ctx->config.instance);

        // the transmit interrupt is only needed while somebody listens; switch it off first so
        // that it never sees a half-updated callback
        CLEAR_BIT(can->IER, CAN_IER_TMEIE);

        ctx->status.tx_callback_cookie = cookie;
        ctx->status.tx_callback = callback;

        if (callback != nullptr) {
            // completions of frames sent before the callback existed are not reported: without
            // this their flags, still set, would raise the interrupt at once
            can->TSR = CAN_TSR_RQCP0 | CAN_TSR_RQCP1 | CAN_TSR_RQCP2;
            SET_BIT(can->IER, CAN_IER_TMEIE);
        }

        result = true;
    }

    return result;
}


bool uni_hal_can_transmit_abort(uni_hal_can_context_t *ctx) {
    bool result = false;
    if (uni_hal_can_is_inited(ctx)) {
        // ABRQ is set by writing 1; the flags in the same register are cleared by writing 1, so
        // only the request bits are written
        _uni_hal_can_get_handle(ctx->config.instance)->TSR = CAN_TSR_ABRQ0 | CAN_TSR_ABRQ1 | CAN_TSR_ABRQ2;
        result = true;
    }
    return result;
}


bool uni_hal_can_transmit(uni_hal_can_context_t *ctx, uni_hal_can_msg_t *msg) {
    bool result = false;

    if (uni_hal_can_is_inited(ctx) && msg != nullptr) {
        uint32_t tx_mailbox = 0U;

        if (_uni_hal_can_queue(ctx, msg, &tx_mailbox)) {
            CAN_TypeDef *can = _uni_hal_can_get_handle(ctx->config.instance);

            // the flags of the three mailboxes sit 8 bits apart in the status register
            uint32_t const tme_mask = CAN_TSR_TME0 << tx_mailbox;
            uint32_t const txok_mask = CAN_TSR_TXOK0 << (8U * tx_mailbox);
            uint32_t const abrq_mask = CAN_TSR_ABRQ0 << (8U * tx_mailbox);

            // wait until the mailbox is done with the frame; in bus-off it never is
            uint32_t const start_ms = uni_hal_systick_get_ms();
            bool timed_out = false;
            while ((can->TSR & tme_mask) == 0U) {
                if ((uni_hal_systick_get_ms() - start_ms) > UNI_HAL_CAN_TX_TIMEOUT_MS) {
                    can->TSR = abrq_mask;
                    timed_out = true;
                    break;
                }
            }

            // The request also completes when the frame was lost (error, arbitration): check TXOK.
            // With a transmit callback registered the interrupt may have acknowledged the
            // request already, which clears TXOK; it keeps the outcome in tx_ok for this case.
            result = !timed_out && ((can->TSR & txok_mask) != 0U || ctx->status.tx_ok[tx_mailbox]);
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

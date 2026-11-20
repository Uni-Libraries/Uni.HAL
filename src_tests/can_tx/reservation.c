// Model the CAN registers and use real Cortex-M interrupt masking and exception entry.
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static bool interrupt_on_selection;
static void interrupt_after_selection(void);
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
static FDCAN_GlobalTypeDef controller;
static uint32_t ram[2560];
#undef FDCAN1
#define FDCAN1 (&controller)
#undef SRAMCAN_BASE
#define SRAMCAN_BASE (interrupt_after_selection(), (uintptr_t)ram)
#define uni_hal_systick_get_ms test_clock
#include "can/uni_hal_can_stm32h7.c"
#else
#include <stm32l4xx.h>
static CAN_TypeDef controller;
#undef CAN1
#define CAN1 (&controller)
// Encoding the ID is the first access to the selected mailbox.
#undef CAN_TI0R_STID_Pos
#define CAN_TI0R_STID_Pos (interrupt_after_selection(), 21U)
#define uni_hal_systick_get_ms test_clock
#include "can/uni_hal_can_stm32l4.c"
#endif

static uni_hal_can_context_t ctx;
static uni_hal_can_msg_t second;
static unsigned pending, fifo_put, callback_count;
static bool nested_success, complete_on_wait, register_on_completion, first_success;
static unsigned last_mailbox;
static bool last_success;
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
static uint32_t registration_ack;
#endif

static void refresh_registers(void) {
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    if ((controller.CCCR & FDCAN_CCCR_INIT) == 0U) {
        controller.CCCR &= ~FDCAN_CCCR_CCE;
    } else if ((controller.CCCR & FDCAN_CCCR_CCE) != 0U) {
        // M_CAN configuration enable resets the TX FIFO and transmission status.
        pending = 0U;
        fifo_put = 0U;
        controller.TXBTO = 0U;
    }
    if (controller.TXBAR != 0U) {
        pending |= controller.TXBAR;
        controller.TXBTO &= ~controller.TXBAR;
        controller.TXBAR = 0U;
        fifo_put = (fifo_put + 1U) % 3U;
    }
    controller.TXBRP = pending;
    unsigned count = 3U - (unsigned)__builtin_popcount(pending);
    controller.TXFQS = (fifo_put << FDCAN_TXFQS_TFQPI_Pos) | count | (count == 0U ? FDCAN_TXFQS_TFQF : 0U);
#else
    unsigned next = 0U;
    for (unsigned i = 0; i < 3; ++i) {
        if (controller.sTxMailBox[i].TIR & CAN_TI0R_TXRQ) {
            pending |= 1U << i;
            controller.sTxMailBox[i].TIR &= ~CAN_TI0R_TXRQ;
        }
    }
    while (next < 3U && (pending & (1U << next))) ++next;
    controller.TSR = ((~pending & 7U) << CAN_TSR_TME0_Pos) | (next << CAN_TSR_CODE_Pos);
#endif
}

static void interrupt_after_selection(void) {
    if (interrupt_on_selection) {
        interrupt_on_selection = false;
        NVIC_SetPendingIRQ((IRQn_Type)0);
        __DSB();
        __ISB();
    }
}

static bool on_completion(void *cookie, uint32_t mailbox, bool success) {
    (void)cookie;
    last_mailbox = mailbox;
    last_success = success;
    ++callback_count;
    refresh_registers();
    nested_success = uni_hal_can_transmit_nowait(&ctx, &second);
    refresh_registers();
    return false;
}

static void complete(unsigned index, bool success) {
    unsigned bit = 1U << index;
    pending &= ~bit;
    refresh_registers();
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    if (success) controller.TXBTO |= bit;
    else controller.TXBTO &= ~bit;
#else
    controller.TSR |= (CAN_TSR_RQCP0 | (success ? CAN_TSR_TXOK0 : 0U)) << (8U * index);
#endif
    if (register_on_completion) {
        // The peripheral finished the waiting sender's frame just before another
        // caller installed the callback. No TX IRQ has serviced this frame.
        register_on_completion = false;
        (void)uni_hal_can_set_tx_callback(&ctx, on_completion, nullptr);
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
        // RAM exposes the W1C command. A completion arriving on another
        // mailbox after the driver's snapshot must survive this write.
        registration_ack = controller.TSR;
#endif
        refresh_registers();
        return;
    }
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    (void)_uni_hal_can_irq_tx(&g_uni_hal_can_instance[0]);
#else
    (void)_uni_hal_can_irq_tx(&ctx, &controller);
#endif
}

static void irq(void) {
    refresh_registers();
    complete(2U, true);
}

uint32_t test_clock(void) {
    static uint32_t now;
    refresh_registers();
    if (complete_on_wait) {
        complete_on_wait = false;
        complete(0U, first_success);
    }
    return ++now;
}

static uni_hal_can_msg_t make_frame(uint8_t tag) {
    uni_hal_can_msg_t msg = {.id = tag, .standard_id = true, .dlc = UNI_HAL_CAN_DATA_SIZE};
#if defined(UNI_HAL_CAN_USE_FD)
    msg.fd = true;
#endif
    for (unsigned i = 0; i < sizeof(msg.data); ++i) msg.data[i] = tag + i;
    return msg;
}

static bool frame_matches(unsigned index, const uni_hal_can_msg_t *frame) {
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    uint32_t *element = &ram[UNI_HAL_CAN_RAM_TX + index * UNI_HAL_CAN_ELEMENT_WORDS];
    return (element[0] >> UNI_HAL_CAN_ELEMENT_STD_POS) == frame->id &&
           memcmp(element + 2, frame->data, frame->dlc) == 0;
#else
    CAN_TxMailBox_TypeDef *box = &controller.sTxMailBox[index];
    return (box->TIR >> 21) == frame->id &&
           memcmp((const void *)&box->TDLR, frame->data, 8) == 0;
#endif
}

static void setup(bool fifo) {
    ctx = (uni_hal_can_context_t){.config = {.instance = UNI_HAL_CORE_PERIPH_CAN_1, .tx_fifo_priority = fifo},
                                 .status = {.inited = true, .tx_callback = on_completion}};
    memset(&controller, 0, sizeof(controller));
    pending = 4U; // An earlier frame causes the callback while the task queues its own.
    fifo_put = callback_count = 0U;
    nested_success = false;
    second = make_frame(0x71);
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    g_uni_hal_can_instance[0].ctx = &ctx;
    g_uni_hal_can_instance[0].tx_inflight = pending;
    ctx.config.fd = second.fd;
#endif
    refresh_registers();
}

#define CHECK(test) do { if (!(test)) return __LINE__; } while (0)

static unsigned check_interrupted_send(bool fifo) {
    setup(fifo);
    uni_hal_can_msg_t first = make_frame(0x31);
    interrupt_on_selection = true;
    CHECK(uni_hal_can_transmit_nowait(&ctx, &first));
    refresh_registers();
    CHECK(nested_success && callback_count == 1U);
    CHECK(pending == 3U && frame_matches(0U, &first) && frame_matches(1U, &second));
    CHECK(ctx.status.count_tx == 2U && uni_hal_can_transmit_free(&ctx) == 1U);
    CHECK(uni_hal_can_transmit_nowait(&ctx, &first));
    refresh_registers();
    CHECK(uni_hal_can_transmit_free(&ctx) == 0U && !uni_hal_can_transmit_nowait(&ctx, &first));
    return 0U;
}

static unsigned check_blocking_result(bool success) {
    setup(false);
    uni_hal_can_msg_t first = make_frame(0x31);
    complete_on_wait = true;
    first_success = success;
    CHECK(uni_hal_can_transmit(&ctx, &first) == success);
    CHECK(nested_success && callback_count == 1U && last_mailbox == 0U && last_success == success);
    CHECK(frame_matches(0U, &first) && frame_matches(1U, &second));
    CHECK(uni_hal_can_transmit_free(&ctx) == 1U);
    return 0U;
}

static unsigned check_callback_registration(bool success) {
    setup(false);
    ctx.status.tx_callback = nullptr;
    complete_on_wait = true;
    register_on_completion = true;
    first_success = success;
    CHECK(uni_hal_can_transmit(&ctx, &second) == success);
    CHECK(callback_count == 0U && ctx.status.tx_callback == on_completion);
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    CHECK(registration_ack == CAN_TSR_RQCP0);
#endif
    complete(2U, true);
    CHECK(last_mailbox == 2U && last_success && callback_count == 1U);
    return 0U;
}

static unsigned check_unreported_completion(void) {
    setup(false);
    // Only slot 0 is idle, but the completion IRQ still owns its result.
    pending = 6U;
    refresh_registers();
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    g_uni_hal_can_instance[0].tx_inflight = 7U;
    controller.TXBTO = 1U;
#else
    controller.TSR |= CAN_TSR_RQCP0 | CAN_TSR_TXOK0;
#endif
    CHECK(uni_hal_can_transmit_free(&ctx) == 0U);
    CHECK(!uni_hal_can_transmit_nowait(&ctx, &second));
    complete(0U, true);
    CHECK(last_mailbox == 0U && last_success && nested_success && pending == 7U);
    return 0U;
}

static unsigned check_timeout(void) {
    setup(false);
    ctx.status.tx_callback = nullptr;
    pending = 0U;
    refresh_registers();
    CHECK(!uni_hal_can_transmit(&ctx, &second));
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    CHECK(controller.TXBCR == 1U);
#else
    CHECK(controller.TSR == CAN_TSR_ABRQ0);
#endif
    CHECK(ctx.status.count_err == 1U && ctx.status.count_tx == 0U);
    // Once the hardware has carried out the cancellation the same slot can be used again.
    complete(0U, false);
    refresh_registers();
    CHECK(uni_hal_can_transmit_free(&ctx) == 3U);
    CHECK(uni_hal_can_transmit_nowait(&ctx, &second));
    refresh_registers();
    CHECK(pending == 1U && frame_matches(0U, &second));
    return 0U;
}

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
static unsigned check_restart(bool fifo) {
    setup(fifo);
    pending = 0U;
    g_uni_hal_can_instance[0].tx_inflight = 0U;
    refresh_registers();
    for (unsigned i = 0; i < 3U; ++i) {
        CHECK(uni_hal_can_transmit_nowait(&ctx, &second));
        refresh_registers();
    }
    CHECK(pending == 7U);
    CHECK(uni_hal_can_stop(&ctx));
    refresh_registers();
    CHECK(!uni_hal_can_transmit_nowait(&ctx, &second));
    // The first start times out waiting for bus idle. Retry once the bus synchronizes.
    CHECK(!uni_hal_can_start(&ctx));
    refresh_registers();
    controller.PSR = 1U << FDCAN_PSR_ACT_Pos;
    CHECK(uni_hal_can_start(&ctx));
    CHECK(pending == 7U && uni_hal_can_transmit_free(&ctx) == 0U);
    complete(0U, true);
    CHECK(callback_count == 1U && last_mailbox == 0U && last_success);
    CHECK(nested_success && pending == 7U);
    return 0U;
}

static unsigned check_fifo_wrap(void) {
    setup(true);
    pending = 0U;
    g_uni_hal_can_instance[0].tx_inflight = 0U;
    fifo_put = 2U;
    refresh_registers();
    uni_hal_can_msg_t first = make_frame(0x31);
    CHECK(uni_hal_can_transmit_nowait(&ctx, &first));
    refresh_registers();
    CHECK(uni_hal_can_transmit_nowait(&ctx, &second));
    refresh_registers();
    CHECK(pending == 5U && frame_matches(2U, &first) && frame_matches(0U, &second));
    // Slot 2 has completed but its IRQ is pending: the FIFO may only use slot 1 before it.
    pending = 1U;
    refresh_registers();
    CHECK(uni_hal_can_transmit_free(&ctx) == 1U);
    CHECK(uni_hal_can_transmit_nowait(&ctx, &second));
    refresh_registers();
    CHECK(uni_hal_can_transmit_free(&ctx) == 0U && !uni_hal_can_transmit_nowait(&ctx, &second));
    complete(2U, true);
    CHECK(last_mailbox == 2U && last_success && nested_success && pending == 7U);
    return 0U;
}
#endif

extern char _stack_top;
void run_tests(void);
__attribute__((used, section(".test_vectors")))
const uintptr_t vectors[17] = {[0] = (uintptr_t)&_stack_top, [1] = (uintptr_t)run_tests, [16] = (uintptr_t)irq};

void run_tests(void) {
    NVIC_EnableIRQ((IRQn_Type)0);
    unsigned failure = check_interrupted_send(false);
    if (!failure) failure = check_interrupted_send(true);
    if (!failure) failure = check_blocking_result(true);
    if (!failure) failure = check_blocking_result(false);
    if (!failure) failure = check_callback_registration(true);
    if (!failure) failure = check_callback_registration(false);
    if (!failure) failure = check_unreported_completion();
    if (!failure) failure = check_timeout();
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    if (!failure) failure = check_fifo_wrap();
    if (!failure) failure = check_restart(false);
    if (!failure) failure = check_restart(true);
#endif
    uint32_t result[] = {0x20026U, failure};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = result;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

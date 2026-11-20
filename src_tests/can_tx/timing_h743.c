#include <stm32h7xx.h>
#include "can/uni_hal_can_internal.h"
#include "rcc/uni_hal_rcc.h"

static FDCAN_GlobalTypeDef controller;
static uint32_t ram[2560];
static uint32_t kernel_clock_hz;

static QueueHandle_t model_rx_queue(QueueHandle_t *queue) {
    static StaticQueue_t rx_queue;
    *queue = (QueueHandle_t)&rx_queue;
    return *queue;
}

#undef FDCAN1
#define FDCAN1 (&controller)
#undef SRAMCAN_BASE
#define SRAMCAN_BASE ((uintptr_t)ram)
#define uni_hal_rcc_clksrc_set(target, source) false
#define uni_hal_rcc_clk_set(target, enable) ((target) == UNI_HAL_CORE_PERIPH_CAN_1 && (enable))
#define uni_hal_rcc_clk_get_freq(target) ((target) == UNI_HAL_CORE_PERIPH_CAN_1 ? kernel_clock_hz : 0U)
#define uni_hal_gpio_pin_init(pin) ((pin) != nullptr)
#define uni_hal_can_init_rx_queue model_rx_queue
#include "can/uni_hal_can_stm32h7.c"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    static uni_hal_gpio_pin_context_t rx, tx;
    static uni_hal_can_context_t ctx = {
        .config = {.instance = UNI_HAL_CORE_PERIPH_CAN_1, .pin_rx = &rx, .pin_tx = &tx, .bitrate = 1'000'000U},
    };
    // A classic frame ignores the optional data rate.
    ctx.config.bitrate_data = 500'000U;
    kernel_clock_hz = 37'000'000U;
    uni_hal_can_timing_t bxcan;
    CHECK(!uni_hal_can_timing_calc(kernel_clock_hz, ctx.config.bitrate, &bxcan));
    CHECK(uni_hal_can_init(&ctx));
    // 37 time quanta, sample after 32 of them (nearest to 87.5%).
    CHECK(controller.NBTP == ((30U << FDCAN_NBTP_NTSEG1_Pos) | (4U << FDCAN_NBTP_NTSEG2_Pos)));

    kernel_clock_hz = 4'000'000U;
    CHECK(uni_hal_can_init(&ctx));
    CHECK(controller.NBTP == (1U << FDCAN_NBTP_NTSEG2_Pos));
    kernel_clock_hz = 3'000'000U;
    CHECK(!uni_hal_can_init(&ctx));

    // The largest representable divisor uses every bit of NBRP, NTSEG1 and NTSEG2.
    kernel_clock_hz = 98'560'000U;
    ctx.config.bitrate = 500U;
    CHECK(uni_hal_can_init(&ctx));
    CHECK(controller.NBTP == (FDCAN_NBTP_NBRP | FDCAN_NBTP_NTSEG1 | FDCAN_NBTP_NTSEG2));
    kernel_clock_hz += ctx.config.bitrate;
    CHECK(!uni_hal_can_init(&ctx));
#if defined(UNI_HAL_CAN_USE_FD)
    // A prime 31 quanta exposes the data phase's independent segment limits.
    kernel_clock_hz = 62'000'000U;
    ctx.config.fd = true;
    ctx.config.bitrate = 1'000'000U;
    ctx.config.bitrate_data = 2'000'000U;
    CHECK(uni_hal_can_init(&ctx));
    CHECK((controller.CCCR & FDCAN_CCCR_BRSE) != 0U);
    CHECK(controller.DBTP == ((23U << FDCAN_DBTP_DTSEG1_Pos) | (5U << FDCAN_DBTP_DTSEG2_Pos) |
                              (5U << FDCAN_DBTP_DSJW_Pos) | FDCAN_DBTP_TDC));
    CHECK(controller.TDCR == (25U << FDCAN_TDCR_TDCO_Pos));

    // RM0433's shortest fast bit: 20 MHz / (1 + 2 + 1) = 5 Mbit/s.
    kernel_clock_hz = 20'000'000U;
    ctx.config.bitrate_data = 5'000'000U;
    CHECK(uni_hal_can_init(&ctx));
    CHECK(controller.DBTP == (FDCAN_DBTP_TDC | (1U << FDCAN_DBTP_DTSEG1_Pos)));
    kernel_clock_hz = 15'000'000U;
    CHECK(!uni_hal_can_init(&ctx));

    kernel_clock_hz = 32'000'000U;
    ctx.config.bitrate_data = 500'000U;
    uint32_t const previous_dbtp = controller.DBTP;
    CHECK(!uni_hal_can_init(&ctx));
    CHECK(controller.DBTP == previous_dbtp);

    ctx.config.bitrate_data = ctx.config.bitrate;
    CHECK(uni_hal_can_init(&ctx));
    CHECK((controller.CCCR & FDCAN_CCCR_BRSE) != 0U);
    ctx.config.bitrate_data = 0U;
    CHECK(uni_hal_can_init(&ctx));
    CHECK((controller.CCCR & (FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE)) == FDCAN_CCCR_FDOE);
#endif
    return 0U;
}

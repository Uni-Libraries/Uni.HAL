#include "model_h743.h"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    uni_hal_gpio_pin_context_t rx = {}, tx = {};
    uni_hal_can_context_t ctx = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_CAN_1, .pin_rx = &rx, .pin_tx = &tx,
        .fd = true, .bitrate = 500'000U, .bitrate_data = 2'000'000U,
    }};
    g_uni_hal_rcc_status.inited = true;
    rcc.CR = RCC_CR_HSERDY;
    CHECK(uni_hal_can_init(&ctx));
    CHECK((FDCAN1->CCCR & (FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE)) == (FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE));
    FDCAN1->PSR = 1U << FDCAN_PSR_ACT_Pos;
    CHECK(uni_hal_can_start(&ctx));
    const uni_hal_can_msg_t msg = {.id = 17, .dlc = 12, .standard_id = true, .fd = true, .brs = true};
    CHECK(uni_hal_can_transmit_nowait(&ctx, &msg));
    CHECK(ram[UNI_HAL_CAN_RAM_TX + 1U] & UNI_HAL_CAN_ELEMENT_BRS);

    uni_hal_can_bus_status_t status;
    // During the switched data phase a CRC error updates DLEC and leaves LEC at NoChange.
    FDCAN1->PSR = FDCAN_PSR_LEC | (6U << FDCAN_PSR_DLEC_Pos);
    CHECK(uni_hal_can_bus_status_get(&ctx, &status) && status.last_error == UNI_HAL_CAN_BUS_ERROR_CRC);
    FDCAN1->PSR = FDCAN_PSR_LEC | FDCAN_PSR_DLEC; // hardware sets both to 7 on read
    CHECK(uni_hal_can_bus_status_get(&ctx, &status) && status.last_error == UNI_HAL_CAN_BUS_ERROR_NONE);

    // An internal read captures an error in either phase. A later read can supersede it.
    FDCAN1->PSR = FDCAN_PSR_EW | 3U | FDCAN_PSR_DLEC;
    FDCAN1->IR = FDCAN_IR_EW;
    FDCAN1_IT0_IRQHandler();
    FDCAN1->PSR = FDCAN_PSR_EP | FDCAN_PSR_LEC | (2U << FDCAN_PSR_DLEC_Pos);
    FDCAN1->IR = FDCAN_IR_EP;
    FDCAN1_IT0_IRQHandler();
    FDCAN1->PSR = FDCAN_PSR_LEC | FDCAN_PSR_DLEC;
    CHECK(uni_hal_can_bus_status_get(&ctx, &status) && status.last_error == UNI_HAL_CAN_BUS_ERROR_FORM);

    // The two hardware latches provide no ordering when both change before a read.
    FDCAN1->PSR = 3U | (6U << FDCAN_PSR_DLEC_Pos);
    CHECK(uni_hal_can_bus_status_get(&ctx, &status) && status.last_error == UNI_HAL_CAN_BUS_ERROR_ACK);
    FDCAN1->PSR = 0U;
    CHECK(uni_hal_can_bus_status_get(&ctx, &status) && status.last_error == UNI_HAL_CAN_BUS_ERROR_NONE);
    return 0U;
}

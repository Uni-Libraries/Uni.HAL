#include "model_h743.h"
#include "../arm/semihost_test.h"

static bool callback_ran;
static uni_hal_can_bus_status_t callback_status;

static void after_status_read(FDCAN_GlobalTypeDef *can) {
    // RM0433: each read of PSR, including checking EW or ACT, resets LEC to NoChange.
    can->PSR |= FDCAN_PSR_LEC;
}

static bool inspect_error(void *cookie, uint32_t errors) {
    after_status_read(FDCAN1);
    callback_ran = (errors == UNI_HAL_CAN_ERROR_WARNING) &&
                   uni_hal_can_bus_status_get(cookie, &callback_status);
    return false;
}

static unsigned check(void) {
    uni_hal_gpio_pin_context_t rx = {}, tx = {};
    uni_hal_can_context_t first = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_CAN_1, .pin_rx = &rx, .pin_tx = &tx, .bitrate = 500'000U,
    }};
    uni_hal_can_context_t second = {.config = first.config};
    second.config.instance = UNI_HAL_CORE_PERIPH_CAN_2;
    g_uni_hal_rcc_status.inited = true;
    rcc.CR = RCC_CR_HSERDY;
    CHECK(uni_hal_can_init(&first) && uni_hal_can_init(&second));
    FDCAN1->PSR = FDCAN2->PSR = 1U << FDCAN_PSR_ACT_Pos;
    CHECK(uni_hal_can_start(&first) && uni_hal_can_start(&second));

    // The warning IRQ reads the ACK error before calling the application's error handler.
    CHECK(uni_hal_can_set_error_callback(&first, inspect_error, &first));
    FDCAN1->PSR = FDCAN_PSR_EW | 3U;
    FDCAN1->IR = FDCAN_IR_EW;
    FDCAN1_IT0_IRQHandler();
    CHECK(callback_ran && callback_status.state == UNI_HAL_CAN_BUS_STATE_WARNING);
    CHECK(callback_status.last_error == UNI_HAL_CAN_BUS_ERROR_ACK);
    uni_hal_can_bus_status_t status;
    CHECK(uni_hal_can_bus_status_get(&first, &status) && status.last_error == UNI_HAL_CAN_BUS_ERROR_NONE);
    CHECK(uni_hal_can_set_error_callback(&first, nullptr, nullptr));

    // A later hardware error supersedes the one consumed by an internal read.
    FDCAN1->PSR = FDCAN_PSR_EP | 2U;
    FDCAN1->IR = FDCAN_IR_EP;
    FDCAN1_IT0_IRQHandler();
    after_status_read(FDCAN1);
    CHECK(uni_hal_can_bus_status_get(&second, &status) && status.last_error == UNI_HAL_CAN_BUS_ERROR_NONE);
    FDCAN1->PSR = FDCAN_PSR_EP | 6U;
    CHECK(uni_hal_can_bus_status_get(&first, &status) && status.last_error == UNI_HAL_CAN_BUS_ERROR_CRC);
    after_status_read(FDCAN1);
    CHECK(uni_hal_can_bus_status_get(&first, &status) && status.last_error == UNI_HAL_CAN_BUS_ERROR_NONE);

    // Starting a stopped controller also reads PSR while waiting for bus synchronization.
    CHECK(uni_hal_can_stop(&second));
    FDCAN2->PSR = (1U << FDCAN_PSR_ACT_Pos) | 1U;
    CHECK(uni_hal_can_start(&second));
    after_status_read(FDCAN2);
    CHECK(uni_hal_can_bus_status_get(&second, &status) && status.last_error == UNI_HAL_CAN_BUS_ERROR_STUFF);

    // An unconsumed error from the previous initialization is not reported for a new session.
    FDCAN1->PSR = FDCAN_PSR_EW | 3U;
    FDCAN1->IR = FDCAN_IR_EW;
    FDCAN1_IT0_IRQHandler();
    after_status_read(FDCAN1);
    CHECK(uni_hal_can_stop(&first) && uni_hal_can_init(&first));
    CHECK(uni_hal_can_bus_status_get(&first, &status) && status.last_error == UNI_HAL_CAN_BUS_ERROR_NONE);
    return 0U;
}

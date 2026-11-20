// Real FDCAN clock selection and bit timing with the RCC, CAN registers and message RAM in RAM.
#include <stm32h7xx.h>
#include "can/uni_hal_can_internal.h"

static RCC_TypeDef rcc;
static FDCAN_GlobalTypeDef controllers[2];
static uint32_t ram[2560];
#undef RCC_BASE
#define RCC_BASE ((uintptr_t)&rcc)
#undef FDCAN1
#undef FDCAN2
#define FDCAN1 (&controllers[0])
#define FDCAN2 (&controllers[1])
#undef SRAMCAN_BASE
#define SRAMCAN_BASE ((uintptr_t)ram)

#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_rcc.c"
#include "rcc/uni_hal_rcc_stm32h7.c"

static uni_hal_can_context_t *contender;
static bool contender_inited;
static bool init_pin(uni_hal_gpio_pin_context_t *pin) {
    if (contender != nullptr) {
        uni_hal_can_context_t *other = contender;
        contender = nullptr;
        contender_inited = uni_hal_can_init(other);
    }
    pin->inited = true;
    return true;
}
#define uni_hal_gpio_pin_init init_pin

static QueueHandle_t init_queue(QueueHandle_t *queue) {
    static StaticQueue_t storage;
    *queue = (QueueHandle_t)&storage;
    return *queue;
}
#define uni_hal_can_init_rx_queue init_queue
// Exercise a board that reserves interrupt priorities 0..5 for non-kernel users.
#include <FreeRTOS.h>
#if configMAX_SYSCALL_INTERRUPT_PRIORITY < (6U << (8U - __NVIC_PRIO_BITS))
#undef configMAX_SYSCALL_INTERRUPT_PRIORITY
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (6U << (8U - __NVIC_PRIO_BITS))
#endif
#include "can/uni_hal_can_stm32h7.c"
#include "../arm/semihost_test.h"

static bool complete(void *cookie, uint32_t slot, bool success) {
    (void)cookie;
    (void)slot;
    (void)success;
    return false;
}

static unsigned check(void) {
    static uni_hal_gpio_pin_context_t rx[2], tx[2];
    static uni_hal_can_context_t first = {
        .config = {.instance = UNI_HAL_CORE_PERIPH_CAN_1, .pin_rx = rx, .pin_tx = tx, .bitrate = 500'000U},
    };
    static uni_hal_can_context_t second = {
        .config = {.instance = UNI_HAL_CORE_PERIPH_CAN_2, .pin_rx = rx + 1, .pin_tx = tx + 1, .bitrate = 500'000U},
    };
    g_uni_hal_rcc_status.inited = true;
    rcc.CR = RCC_CR_HSERDY;

    // An in-progress init adopts the board's HSE. Its preempting sibling requests an offline PLL.
    second.config.clock_source = UNI_HAL_RCC_CLKSRC_PLL1Q;
    contender = &second;
    CHECK(uni_hal_can_init(&first));
    CHECK(!contender_inited && !rx[1].inited && !tx[1].inited);
    CHECK(uni_hal_rcc_clk_get_freq(first.config.instance) == HSE_VALUE);
    CHECK(!uni_hal_can_init(&second));
    CHECK(uni_hal_rcc_clk_get_freq(first.config.instance) == HSE_VALUE);

    second.config.clock_source = UNI_HAL_RCC_CLKSRC_HSE;
    CHECK(uni_hal_can_init(&second));
    uint32_t const expected_priority = configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS);
    CHECK(NVIC_GetPriority(FDCAN1_IT0_IRQn) == expected_priority);
    CHECK(NVIC_GetPriority(FDCAN2_IT0_IRQn) == expected_priority);
    NVIC_SetPriorityGrouping(7U);
    CHECK(!uni_hal_can_init(&second) && uni_hal_can_is_inited(&second));
    NVIC_SetPriorityGrouping(0U);
    CHECK(controllers[0].NBTP == controllers[1].NBTP);
    // A rejected reinit must preserve a stopped controller's pending transmission and callback.
    controllers[0].PSR = 1U << FDCAN_PSR_ACT_Pos;
    CHECK(uni_hal_can_start(&first));
    CHECK(uni_hal_can_set_tx_callback(&first, complete, nullptr));
    const uni_hal_can_msg_t frame = {.standard_id = true, .id = 42U};
    CHECK(uni_hal_can_transmit_nowait(&first, &frame));
    first.config.bitrate = 0U;
    CHECK(!uni_hal_can_init(&first));
    CHECK(uni_hal_can_is_inited(&first) && uni_hal_can_transmit_free(&first) == 2U);
    CHECK(uni_hal_can_stop(&second));
    second.config.clock_source = UNI_HAL_RCC_CLKSRC_PLL1Q;
    CHECK(!uni_hal_can_init(&second));
    CHECK(uni_hal_rcc_clk_get_freq(first.config.instance) == HSE_VALUE);
    second.config.clock_source = UNI_HAL_RCC_CLKSRC_HSE;
    CHECK(uni_hal_can_init(&second));
    first.config.bitrate = 500'000U;
    CHECK(uni_hal_can_stop(&first));
    // A delayed bus-off IRQ must not put the explicitly stopped node back on the bus.
    first.config.auto_bus_off = true;
    controllers[0].PSR |= FDCAN_PSR_BO;
    controllers[0].IR = FDCAN_IR_BO;
    FDCAN1_IT0_IRQHandler();
    CHECK((controllers[0].CCCR & FDCAN_CCCR_INIT) != 0U);
    controllers[0].PSR &= ~FDCAN_PSR_BO;
    first.config.clock_source = UNI_HAL_RCC_CLKSRC_PLL2Q;
    CHECK(!uni_hal_can_init(&first));
    CHECK(uni_hal_rcc_clk_get_freq(second.config.instance) == HSE_VALUE);
    CHECK(uni_hal_can_is_inited(&first) && first.status.count_tx == 1U);
    CHECK(first.status.tx_callback == complete && uni_hal_can_transmit_free(&first) == 2U);
    first.config.clock_source = UNI_HAL_RCC_CLKSRC_UNKNOWN;
    CHECK(uni_hal_can_init(&first));

    // A failed timing calculation releases the claim. Only the remaining sibling sets the mux.
    first.config.bitrate = 0U;
    CHECK(!uni_hal_can_init(&first));
    second.config.clock_source = UNI_HAL_RCC_CLKSRC_PLL2Q;
    CHECK(!uni_hal_can_init(&second)); // PLL2 is off.
    CHECK(uni_hal_rcc_clksrc_get(first.config.instance) == UNI_HAL_RCC_CLKSRC_PLL2Q);
    first.config.bitrate = 500'000U;
    first.config.clock_source = UNI_HAL_RCC_CLKSRC_HSE;
    CHECK(uni_hal_can_init(&first));
    CHECK(uni_hal_rcc_clksrc_get(second.config.instance) == UNI_HAL_RCC_CLKSRC_HSE);
    // After an explicit stop the other controller can select a new clock. Restart must
    // restore the clock its pending frames were timed for or refuse to disturb a live sibling.
    CHECK(uni_hal_can_stop(&first));
    SCB->CPACR |= 0xFU << 20U;
    __DSB();
    __ISB();
    LL_RCC_PLL_SetSource(LL_RCC_PLLSOURCE_HSE);
    LL_RCC_PLL1_SetM(4U);
    LL_RCC_PLL1_SetN(128U);
    LL_RCC_PLL1_SetQ(8U);
    LL_RCC_PLL1Q_Enable();
    rcc.CR |= RCC_CR_PLL1RDY;
    second.config.clock_source = UNI_HAL_RCC_CLKSRC_PLL1Q;
    CHECK(uni_hal_can_init(&second));
    CHECK(uni_hal_rcc_clk_get_freq(second.config.instance) == 4U * HSE_VALUE);
    CHECK(!uni_hal_can_start(&first));
    CHECK((controllers[0].CCCR & FDCAN_CCCR_INIT) != 0U);
    CHECK(uni_hal_rcc_clksrc_get(first.config.instance) == UNI_HAL_RCC_CLKSRC_PLL1Q);
    CHECK(uni_hal_can_stop(&second));
    CHECK(uni_hal_can_start(&first));
    CHECK(uni_hal_rcc_clk_get_freq(first.config.instance) == HSE_VALUE);
    controllers[0].CCCR |= FDCAN_CCCR_INIT;
    controllers[0].PSR |= FDCAN_PSR_BO;
    controllers[0].IR = FDCAN_IR_BO;
    FDCAN1_IT0_IRQHandler();
    CHECK((controllers[0].CCCR & FDCAN_CCCR_INIT) == 0U);
    return 0U;
}

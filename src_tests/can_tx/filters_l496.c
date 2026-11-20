#include <stm32l4xx.h>
#include <stdbool.h>

static CAN_TypeDef controllers[2];
#undef CAN1
#undef CAN2
#define CAN1 (&controllers[0])
#define CAN2 (&controllers[1])

static bool interrupt_on_filter_init;
static void set_bits(volatile uint32_t *reg, uint32_t bits) {
    // RM0351: scale and FIFO assignment are writable only while FINIT is set.
    if ((reg == &CAN1->FS1R || reg == &CAN1->FFA1R) && !(CAN1->FMR & CAN_FMR_FINIT)) {
        return;
    }
    *reg |= bits;
    if (reg == &CAN1->FMR && bits == CAN_FMR_FINIT && interrupt_on_filter_init) {
        interrupt_on_filter_init = false;
        NVIC_SetPendingIRQ(WWDG_IRQn);
        __DSB();
        __ISB();
    }
}
#undef SET_BIT
#define SET_BIT(reg, bits) set_bits(&(reg), (bits))
#include "can/uni_hal_can_stm32l4.c"

_Noreturn void finish(unsigned code);
#define CHECK(condition) do { if (!(condition)) finish(__LINE__); } while (0)

static uni_hal_can_context_t can1, can2;
static TaskHandle_t high, low;

void WWDG_IRQHandler(void) {
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(high, &woken);
    portYIELD_FROM_ISR(woken);
}

static void filter_can2(void *arg) {
    (void)arg;
    CHECK(ulTaskNotifyTake(pdTRUE, portMAX_DELAY));
    CHECK(uni_hal_can_set_filter(&can2, 0U, 14U, 0x456U << 21, 0xffe00000U));
    xTaskNotifyGive(low);
    vTaskDelete(nullptr);
}

static void filter_can1(void *arg) {
    (void)arg;
    interrupt_on_filter_init = true;
    CHECK(uni_hal_can_set_filter(&can1, 1U, 0U, 0x123U << 21, 0xffe00000U));
    CHECK(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)));
    const uint32_t banks = 1U | (1U << 14);
    CHECK((CAN1->FS1R & banks) == banks);
    CHECK((CAN1->FFA1R & banks) == 1U);
    CHECK((CAN1->FA1R & banks) == banks && !(CAN1->FMR & CAN_FMR_FINIT));
    CHECK(CAN1->sFilterRegister[0].FR1 == (0x123U << 21));
    CHECK(CAN1->sFilterRegister[14].FR1 == (0x456U << 21));
    CHECK((CAN1->IER & CAN_IER_FMPIE1) && (CAN2->IER & CAN_IER_FMPIE0));
    finish(0U);
}

int main(void) {
    static uni_hal_gpio_pin_context_t pins[4] = {{.inited = true}, {.inited = true}, {.inited = true}, {.inited = true}};
    can1.config = (uni_hal_can_config_t){.instance = UNI_HAL_CORE_PERIPH_CAN_1, .pin_rx = &pins[0], .pin_tx = &pins[1]};
    can2.config = (uni_hal_can_config_t){.instance = UNI_HAL_CORE_PERIPH_CAN_2, .pin_rx = &pins[2], .pin_tx = &pins[3]};
    CAN1->MSR = CAN2->MSR = CAN_MSR_INAK;
    CHECK(uni_hal_can_init(&can1) && uni_hal_can_init(&can2));
    NVIC_SetPriorityGrouping(0U);
    NVIC_SetPriority(WWDG_IRQn, configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS));
    NVIC_EnableIRQ(WWDG_IRQn);
    CHECK(xTaskCreate(filter_can1, "CAN1", 256U, nullptr, 1U, &low) == pdPASS);
    CHECK(xTaskCreate(filter_can2, "CAN2", 256U, nullptr, 2U, &high) == pdPASS);
    vTaskStartScheduler();
    finish(__LINE__);
}

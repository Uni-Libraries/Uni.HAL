// Deliver PendSV after the LL bus helper reads the RCC register and before it writes it.
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif
#include <stdbool.h>

static RCC_TypeDef rcc;
static RTC_TypeDef rtc;
#undef RTC
#define RTC (&rtc)
#undef RCC
#define RCC (&rcc)
static bool pend_on_update;

static uint32_t read_for_update(volatile uint32_t *reg) {
    uint32_t const value = *reg;
    if (pend_on_update) {
        pend_on_update = false;
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
        __DSB();
        __ISB();
    }
    return value;
}

#pragma push_macro("SET_BIT")
#pragma push_macro("CLEAR_BIT")
#undef SET_BIT
#undef CLEAR_BIT
#define SET_BIT(reg, mask) ((reg) = read_for_update(&(reg)) | (mask))
#define CLEAR_BIT(reg, mask) ((reg) = read_for_update(&(reg)) & ~(mask))
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_bus.h>
#else
#include <stm32l4xx_ll_bus.h>
#endif
#pragma pop_macro("CLEAR_BIT")
#pragma pop_macro("SET_BIT")

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "rcc/uni_hal_rcc_stm32h7.c"
#else
#include "rcc/uni_hal_rcc_stm32l4.c"
#endif
#include "rtc/uni_hal_rtc_stm32.c"
#include "../arm/semihost_test.h"

static uni_hal_core_periph_e neighbor;
static bool neighbor_state;
static volatile bool irq_updated;

static void update_neighbor(void) {
    irq_updated = uni_hal_rcc_clk_set(neighbor, neighbor_state);
}

static unsigned exercise(uni_hal_core_periph_e target, uni_hal_core_periph_e other, bool already_set) {
    neighbor = other;
    for (unsigned enabled = 0; enabled <= 1; ++enabled) {
        for (unsigned masked = 0; masked <= 1; ++masked) {
            CHECK(uni_hal_rcc_clk_set(target, already_set ? enabled : !enabled));
            CHECK(uni_hal_rcc_clk_set(other, enabled));
            neighbor_state = !enabled;
            irq_updated = false;
            pend_on_update = true;
            __set_PRIMASK(masked);
            CHECK(uni_hal_rcc_clk_set(target, enabled));
            CHECK(__get_PRIMASK() == masked);
            if (masked) {
                CHECK(!irq_updated && (SCB->ICSR & SCB_ICSR_PENDSVSET_Msk));
                __enable_irq();
                __ISB();
            }
            CHECK(irq_updated && !pend_on_update);
            CHECK(uni_hal_rcc_clk_get(target) == enabled);
            CHECK(uni_hal_rcc_clk_get(other) == neighbor_state);
        }
    }
    return 0;
}

static unsigned check(void) {
    alignas(1024) static uintptr_t irq_vectors[16];
    irq_vectors[16 + PendSV_IRQn] = (uintptr_t)update_neighbor;
    SCB->VTOR = (uintptr_t)irq_vectors;
    __DSB();
    __ISB();
    unsigned error = exercise(UNI_HAL_CORE_PERIPH_TIM_2, UNI_HAL_CORE_PERIPH_UART_2, false);
    if (error) {
        return error;
    }
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    error = exercise(UNI_HAL_CORE_PERIPH_ETH_1, UNI_HAL_CORE_PERIPH_DMA_1, false);
    neighbor = UNI_HAL_CORE_PERIPH_SYSCFG; // APB4, shared with RTCAPB
#else
    // L496 also writes its bus register on an idempotent request.
    error = exercise(UNI_HAL_CORE_PERIPH_TIM_2, UNI_HAL_CORE_PERIPH_UART_2, true);
    neighbor = UNI_HAL_CORE_PERIPH_UART_2; // APB1, shared with PWR and RTCAPB
#endif
    if (error) {
        return error;
    }
    CHECK(uni_hal_rcc_clk_set(neighbor, false));
    neighbor_state = true;
    pend_on_update = true;
    irq_updated = false;
    rtc.BKP0R = 0x12345678U;
    CHECK(uni_hal_rtc_backup_read(0) == rtc.BKP0R);
    CHECK(irq_updated && uni_hal_rcc_clk_get(neighbor));
    return 0;
}

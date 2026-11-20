// Run the GPIO driver with EXTI/SYSCFG in SRAM and the Cortex-M NVIC delivering IRQs.
#include <stdbool.h>
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif

static EXTI_TypeDef exti;
static SYSCFG_TypeDef syscfg;
static GPIO_TypeDef gpio_a;
#undef EXTI
#undef GPIOA
#define GPIOA (&gpio_a)
#undef SYSCFG
#define EXTI (&exti)
#define SYSCFG (&syscfg)

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_system.h>
#include "../../3rdparty/st_stm32h7/src_hal/stm32h7xx_ll_exti.c"
#else
#include <stm32l4xx_ll_system.h>
#include "../../3rdparty/st_stm32l4/src_hal/stm32l4xx_ll_exti.c"
#endif

static bool edge_before_route, edge_before_disable;
static void raise_edges(uint32_t lines) {
    exti.PR1 |= lines;
    if ((lines & exti.IMR1) != 0U) {
        NVIC_SetPendingIRQ(EXTI9_5_IRQn);
        __DSB();
        __ISB();
    }
}

static void route_line(uint32_t port, uint32_t line) {
    if (edge_before_route) {
        edge_before_route = false;
        // PA5 and PA6 rise together, just before the route changes from PA5 to PB5.
        raise_edges(LL_EXTI_LINE_5 | LL_EXTI_LINE_6);
    }
    LL_SYSCFG_SetEXTISource(port, line);
}

static void disable_line(uint32_t line) {
    if (edge_before_disable) {
        edge_before_disable = false;
        raise_edges(LL_EXTI_LINE_6);
    }
    LL_EXTI_DisableIT_0_31(line);
}

#define LL_EXTI_DisableIT_0_31 disable_line
#define LL_SYSCFG_SetEXTISource route_line
#define LL_EXTI_ClearFlag_0_31(line) (exti.PR1 &= ~(line)) // W1C pending flags in SRAM
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "gpio/uni_hal_gpio_stm32h7.c"
#else
#include "gpio/uni_hal_gpio_stm32l4.c"
#endif
#include "../arm/semihost_test.h"

typedef struct {
    volatile unsigned calls;
} counter_t;

static counter_t old_edges, new_edges, neighbor_edges;
static uni_hal_gpio_pin_context_t *next_owner;

static bool count_edge(void *cookie) {
    ++((counter_t *)cookie)->calls;
    return false;
}

static bool neighbor_callback(void *cookie) {
    count_edge(cookie);
    if (next_owner) {
        uni_hal_gpio_pin_set_interrupt_callback(next_owner, UNI_HAL_GPIO_IT_EDGE_RISING, count_edge, &new_edges);
    }
    return false;
}

static unsigned check(void) {
    alignas(1024) static uintptr_t irq_vectors[16U + EXTI9_5_IRQn + 1U];
    irq_vectors[16U + EXTI9_5_IRQn] = (uintptr_t)EXTI9_5_IRQHandler;
    SCB->VTOR = (uintptr_t)irq_vectors;
    __DSB();
    __ISB();

    uni_hal_gpio_pin_context_t pa5 = {.gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_A, .gpio_pin = UNI_HAL_GPIO_PIN_5, .inited = true};
    uni_hal_gpio_pin_context_t pa6 = {.gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_A, .gpio_pin = UNI_HAL_GPIO_PIN_6, .inited = true};
    uni_hal_gpio_pin_context_t pb5 = {.gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_B, .gpio_pin = UNI_HAL_GPIO_PIN_5, .inited = true};
    CHECK(uni_hal_gpio_pin_set_interrupt_callback(&pa5, UNI_HAL_GPIO_IT_EDGE_RISING, count_edge, &old_edges));
    CHECK(uni_hal_gpio_pin_set_interrupt_callback(&pa6, UNI_HAL_GPIO_IT_EDGE_RISING, neighbor_callback, &neighbor_edges));
    raise_edges(LL_EXTI_LINE_5);
    CHECK(old_edges.calls == 1U);

    edge_before_route = true;
    CHECK(uni_hal_gpio_pin_set_interrupt_callback(&pb5, UNI_HAL_GPIO_IT_EDGE_RISING, count_edge, &new_edges));
    CHECK(new_edges.calls == 0U && neighbor_edges.calls == 1U);
    CHECK(LL_SYSCFG_GetEXTISource(LL_SYSCFG_EXTI_LINE5) == LL_SYSCFG_EXTI_PORTB);
    raise_edges(LL_EXTI_LINE_5);
    CHECK(new_edges.calls == 1U);

    // Replacing the callback while its caller already masks IRQs must preserve that mask.
    uint32_t const mask = uni_hal_core_irq_pause();
    CHECK(uni_hal_gpio_pin_set_interrupt_callback(&pa5, UNI_HAL_GPIO_IT_EDGE_FALLING, count_edge, &old_edges));
    CHECK(__get_PRIMASK() != 0U);
    uni_hal_core_irq_resume(mask);

    // An IRQ on the shared vector assigns PB5 while PA5 is being released.
    next_owner = &pb5;
    edge_before_disable = true;
    CHECK(uni_hal_gpio_pin_deinit(&pa5));
    CHECK(neighbor_edges.calls == 2U);
    raise_edges(LL_EXTI_LINE_5);
    CHECK(new_edges.calls == 2U);
    return 0;
}

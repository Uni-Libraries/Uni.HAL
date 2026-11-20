#include "model.h"
#include "../arm/semihost_test.h"

void TIM2_IRQHandler(void);
static volatile unsigned interrupts;
static volatile unsigned callbacks;

static void tim2_vector(void) {
    ++interrupts;
    TIM2_IRQHandler();
}

static bool on_update(void *timer, void *user) {
    (void)timer;
    (void)user;
    ++callbacks;
    return false;
}

static unsigned check(void) {
    static uni_hal_tim_context_t ctx = {.config = {.instance = UNI_HAL_CORE_PERIPH_TIM_2, .reload_value = 99U}};
    alignas(1024) static uintptr_t irq_vectors[16U + TIM2_IRQn + 1U];
    irq_vectors[16U + TIM2_IRQn] = (uintptr_t)tim2_vector;
    SCB->VTOR = (uintptr_t)irq_vectors;
    __DSB();
    __ISB();

    CHECK(uni_hal_tim_init(&ctx) && uni_hal_tim_register_callback(&ctx, on_update, nullptr));
    CHECK(uni_hal_tim_start(&ctx));
    uint32_t mask = uni_hal_core_irq_pause();
    model_tim2.SR = TIM_SR_UIF;
    NVIC_SetPendingIRQ(TIM2_IRQn);
    CHECK(NVIC_GetPendingIRQ(TIM2_IRQn) && interrupts == 0U);
    CHECK(uni_hal_tim_stop(&ctx));
    uni_hal_core_irq_resume(mask);
    __ISB();
    CHECK(interrupts == 1U);
    CHECK(callbacks == 0U);

    CHECK(uni_hal_tim_start(&ctx));
    CHECK(!LL_TIM_IsActiveFlag_UPDATE(&model_tim2));
    model_tim2.SR = TIM_SR_UIF;
    NVIC_SetPendingIRQ(TIM2_IRQn);
    __DSB();
    __ISB();
    CHECK(interrupts == 2U && callbacks == 1U && !LL_TIM_IsActiveFlag_UPDATE(&model_tim2));
    return 0;
}

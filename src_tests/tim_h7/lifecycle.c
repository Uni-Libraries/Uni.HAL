#include "model.h"
#include "../arm/semihost_test.h"

void TIM2_IRQHandler(void);
void TIM15_IRQHandler(void);
void TIM16_IRQHandler(void);
void TIM17_IRQHandler(void);

#if defined(TEST_TIM2_APPLICATION_VECTOR)
void TIM2_IRQHandler(void) {
    (void)uni_hal_tim_period_elapsed(UNI_HAL_CORE_PERIPH_TIM_2);
}
#endif

static uni_hal_tim_context_t *callback_timer;
static void *callback_user;
static unsigned callbacks;

static bool on_update(void *timer, void *user) {
    callback_timer = timer;
    callback_user = user;
    ++callbacks;
    return false;
}

static __attribute__((noinline)) unsigned test_timer(void) {
    static const struct {
        uni_hal_core_periph_e instance;
        TIM_TypeDef *registers;
        uni_hal_core_irq_e irq;
        IRQn_Type irqn;
        void (*handler)(void);
    } timers[] = {
        {UNI_HAL_CORE_PERIPH_TIM_2, &model_tim2, UNI_HAL_CORE_IRQ_TIM_2, TIM2_IRQn, TIM2_IRQHandler},
        {UNI_HAL_CORE_PERIPH_TIM_15, &model_tim15, UNI_HAL_CORE_IRQ_TIM_15, TIM15_IRQn, TIM15_IRQHandler},
        {UNI_HAL_CORE_PERIPH_TIM_16, &model_tim16, UNI_HAL_CORE_IRQ_TIM_16, TIM16_IRQn, TIM16_IRQHandler},
        {UNI_HAL_CORE_PERIPH_TIM_17, &model_tim17, UNI_HAL_CORE_IRQ_TIM_17, TIM17_IRQn, TIM17_IRQHandler},
    };
    static uni_hal_tim_context_t contexts[sizeof timers / sizeof timers[0]];
    for (unsigned i = 0; i < sizeof timers / sizeof timers[0]; ++i) {
        uni_hal_tim_context_t *ctx = &contexts[i];
        ctx->config.instance = timers[i].instance;
        ctx->config.prescaler = 3U;
        ctx->config.reload_value = 59U;
        CHECK(uni_hal_tim_init(ctx) && uni_hal_tim_is_inited(ctx));
        CHECK(uni_hal_tim_register_callback(ctx, on_update, timers[i].registers));
        CHECK(uni_hal_tim_start(ctx));
        CHECK(uni_hal_core_irq_getnum(timers[i].irq) == (uint32_t)timers[i].irqn);
        CHECK(LL_TIM_IsEnabledCounter(timers[i].registers) && LL_TIM_IsEnabledIT_UPDATE(timers[i].registers));
        CHECK(timers[i].registers->PSC == 3U && timers[i].registers->ARR == 59U);
    }
    CHECK(NVIC_GetEnableIRQ(TIM2_IRQn));
    for (unsigned i = 0; i < sizeof timers / sizeof timers[0]; ++i) {
        timers[i].registers->SR = TIM_SR_UIF;
        timers[i].handler();
        timers[i].handler();
        CHECK(callbacks == i + 1U && callback_timer == &contexts[i] && callback_user == timers[i].registers);
        CHECK(uni_hal_tim_stop(&contexts[i]));
        CHECK(!LL_TIM_IsEnabledIT_UPDATE(timers[i].registers) && !LL_TIM_IsEnabledCounter(timers[i].registers));
    }

    uni_hal_tim_context_t ctx = {.config = {.instance = UNI_HAL_CORE_PERIPH_TIM_3}};
    CHECK(!uni_hal_tim_init(&ctx) && !uni_hal_tim_is_inited(&ctx));
    ctx.config.instance = UNI_HAL_CORE_PERIPH_GPIO_A;
    CHECK(!uni_hal_tim_init(&ctx) && clock_calls == 4U);
    CHECK(!uni_hal_tim_register_callback(&ctx, on_update, nullptr) && !uni_hal_tim_start(&ctx));
    ctx.config.instance = UNI_HAL_CORE_PERIPH_TIM_15;
    clock_failed = true;
    CHECK(!uni_hal_tim_init(&ctx) && !uni_hal_tim_is_inited(&ctx));
    CHECK(clock_calls == 5U && model_tim15.ARR == 59U);
    return 0;
}

static unsigned check(void) {
    // The test vector bypasses SystemInit. Enable the FPU before entering the hard-float test.
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    __DSB();
    __ISB();
    return test_timer();
}

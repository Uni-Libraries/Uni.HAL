#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "../tim_h7/model.h"
#else
#include "../tim_l4/model.h"
#endif
#include "../arm/semihost_test.h"

void TIM2_IRQHandler(void);

static uni_hal_tim_context_t *last_timer;
static void *last_user;
static unsigned configured_calls, registered_calls;

static bool configured(void *timer, void *user) {
    ++configured_calls;
    last_timer = timer;
    last_user = user;
    return false;
}

static bool registered(void *timer, void *user) {
    ++registered_calls;
    last_timer = timer;
    last_user = user;
    return false;
}

static void update(void) {
    model_tim2.SR = TIM_SR_UIF;
    TIM2_IRQHandler();
}

static unsigned check(void) {
    static uni_hal_tim_context_t ctx = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_TIM_2, .callback = configured, .reload_value = 99U,
    }};
    CHECK(uni_hal_tim_init(&ctx) && uni_hal_tim_start(&ctx));
    update();
    CHECK(configured_calls == 1U && last_timer == &ctx && last_user == nullptr);

    CHECK(uni_hal_tim_register_callback(&ctx, registered, &model_tim2));
    update();
    CHECK(configured_calls == 1U && registered_calls == 1U && last_user == &model_tim2);
    CHECK(uni_hal_tim_register_callback(&ctx, nullptr, nullptr));
    update();
    CHECK(configured_calls == 1U && registered_calls == 1U);
    CHECK(uni_hal_tim_stop(&ctx));

    CHECK(uni_hal_tim_init(&ctx) && uni_hal_tim_start(&ctx));
    update();
    CHECK(configured_calls == 2U && last_timer == &ctx && last_user == nullptr);
    CHECK(uni_hal_tim_stop(&ctx));

    static uni_hal_tim_context_t replacement = {.config = {.instance = UNI_HAL_CORE_PERIPH_TIM_2}};
    CHECK(uni_hal_tim_init(&replacement) && uni_hal_tim_start(&replacement));
    update();
    CHECK(configured_calls == 2U && registered_calls == 1U);
    return 0;
}

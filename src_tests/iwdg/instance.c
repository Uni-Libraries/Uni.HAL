#include "model.h"
#include "../arm/semihost_test.h"

static unsigned check(void) {
    uni_hal_iwdg_context_t ctx = {
        .instance = UNI_HAL_CORE_PERIPH_IWDG_2,
        .watchdog_prescaler = 64,
        .watchdog_counter = 1024,
    };
    watchdog.PR = LL_IWDG_PRESCALER_32;
    watchdog.RLR = 319;
    CHECK(!uni_hal_iwdg_init(&ctx));
    CHECK(!uni_hal_iwdg_is_inited(&ctx));
    CHECK(watchdog.KR == 0 && watchdog.PR == LL_IWDG_PRESCALER_32 && watchdog.RLR == 319);
    CHECK(reloads == 0);

    ctx.instance = UNI_HAL_CORE_PERIPH_IWDG_1;
    CHECK(uni_hal_iwdg_init(&ctx));
    CHECK(watchdog.PR == LL_IWDG_PRESCALER_64 && watchdog.RLR == 1024 && reloads == 1);
    ctx.instance = UNI_HAL_CORE_PERIPH_UNKNOWN;
    CHECK(!uni_hal_iwdg_reload(&ctx));
    CHECK(reloads == 1);
    ctx.instance = UNI_HAL_CORE_PERIPH_IWDG_1;
    CHECK(uni_hal_iwdg_reload(&ctx) && reloads == 2);
    return 0;
}

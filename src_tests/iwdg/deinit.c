#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif

static IWDG_TypeDef watchdog;
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#undef IWDG1
#define IWDG1 (&watchdog)
#include "iwdg/uni_hal_iwdg_stm32h7.c"
#else
#undef IWDG
#define IWDG (&watchdog)
#include "iwdg/uni_hal_iwdg_stm32l4.c"
#endif
#include "../arm/semihost_test.h"

static unsigned check(void) {
    uni_hal_iwdg_context_t ctx = {
        .instance = UNI_HAL_CORE_PERIPH_IWDG_1,
        .watchdog_prescaler = 32,
        .watchdog_counter = 319,
    };
    CHECK(uni_hal_iwdg_init(&ctx));
    CHECK(uni_hal_iwdg_is_inited(&ctx));
    watchdog.KR = 0;
    CHECK(!uni_hal_iwdg_deinit(&ctx));
    CHECK(uni_hal_iwdg_is_inited(&ctx));
    CHECK(watchdog.PR == LL_IWDG_PRESCALER_32 && watchdog.RLR == 319U);
    CHECK(watchdog.KR == 0);
    CHECK(uni_hal_iwdg_reload(&ctx));
    CHECK(watchdog.KR == LL_IWDG_KEY_RELOAD);
    return 0;
}

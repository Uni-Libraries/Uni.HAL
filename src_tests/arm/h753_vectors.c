#include "stm32h7xx.h"
#include "semihost_test.h"

extern void (*const g_pfnVectors[])(void);

// The MPS2 board boots the test harness. Keep the STM32 vector table linked
// without executing the device-specific power and clock startup on this board.
void Reset_Handler(void) {
    run_tests();
}

static volatile unsigned handled_irq;
void CRYP_IRQHandler(void) {
    handled_irq = CRYP_IRQn;
}

void HASH_RNG_IRQHandler(void) {
    handled_irq = HASH_RNG_IRQn;
}

static unsigned check(void) {
    static_assert(__CORTEX_M == 7U);
    CHECK(g_pfnVectors[16 + CRYP_IRQn] != nullptr);
    g_pfnVectors[16 + CRYP_IRQn]();
    CHECK(handled_irq == CRYP_IRQn);
    CHECK(g_pfnVectors[16 + HASH_RNG_IRQn] != nullptr);
    g_pfnVectors[16 + HASH_RNG_IRQn]();
    CHECK(handled_irq == HASH_RNG_IRQn);
    return 0;
}

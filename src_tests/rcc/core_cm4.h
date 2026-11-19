// Host register qualifiers required by ST's STM32L496 device header.
#pragma once
#include <stdint.h>

#define __IO volatile
#define __I volatile const
#define __O volatile
#define __STATIC_INLINE static inline

typedef struct {
    uint32_t CTRL;
    uint32_t LOAD;
    uint32_t VAL;
} rcc_test_systick_t;

#ifdef __cplusplus
extern "C" {
#endif
extern rcc_test_systick_t rcc_test_systick;
#define SysTick (&rcc_test_systick)
#define SysTick_CTRL_CLKSOURCE_Msk (1U << 2)
#define SysTick_LOAD_RELOAD_Msk 0xFFFFFFU

// Referenced only by uncalled EXTI inline functions in the vendor headers.
uint32_t __CLZ(uint32_t value);
uint32_t __RBIT(uint32_t value);
#ifdef __cplusplus
}
#endif

static inline void __DMB(void) {}

//
// Includes
//

// stdlib
#include <stdint.h>

// cmsis
#include <cmsis_gcc.h>

// uni_hal
#include "systick/uni_hal_systick.h"



//
// Extern
//

extern volatile uint32_t g_uni_hal_systick_counter;



//
// Functions
//

uint32_t uni_hal_systick_get_ms(void){
    return g_uni_hal_systick_counter / (uni_hal_systick_get_freq() / 1000);
}


uint32_t uni_hal_systick_get_tick(void){
    return g_uni_hal_systick_counter;
}


uint32_t uni_hal_systick_get_freq(void) {
    return 1000U;
}


void uni_hal_systick_delay(uint32_t val) {
    uint32_t tick_start = uni_hal_systick_get_tick();
    if (val == 0) {
        val = 1;
    }
    while ((uni_hal_systick_get_tick() - tick_start) < val) {
        __WFI();
    }
}

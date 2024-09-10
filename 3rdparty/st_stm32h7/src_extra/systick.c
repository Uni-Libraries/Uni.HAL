#include <stdint.h>


volatile uint32_t g_uni_hal_systick_counter = 0U;


uint32_t HAL_GetTick(void) {
    return g_uni_hal_systick_counter;
}

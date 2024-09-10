//
// Includes
//

// stdlib
#include <time.h>

// uni_hal
#include "systick/uni_hal_systick.h"



//
// Functions
//

uint32_t uni_hal_systick_get_ms(void){
    struct timespec     up;
    clock_gettime(CLOCK_BOOTTIME, &up);
    return up.tv_sec * 1000 + up.tv_nsec / 1000000;
}

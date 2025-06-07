//
// Includes
//

// stdlib
#include <time.h>

// windows
#if defined(_WIN32)
#include <windows.h>
#endif

// uni_hal
#include "systick/uni_hal_systick.h"



//
// Functions
//

uint32_t uni_hal_systick_get_ms(void){
#if defined(_WIN32)
    return (uint32_t)GetTickCount64();
#else
    struct timespec     up;
    clock_gettime(CLOCK_BOOTTIME, &up);
    return up.tv_sec * 1000 + up.tv_nsec / 1000000;
#endif
}

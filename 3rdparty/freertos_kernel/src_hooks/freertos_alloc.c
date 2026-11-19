#include <FreeRTOS.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

void vApplicationMallocFailedHook( void )
{
    volatile uint32_t c = 0;
    while (!c) {
#if defined(_MSC_VER)
        __debugbreak();
#else
        __builtin_trap();
#endif
    }
}

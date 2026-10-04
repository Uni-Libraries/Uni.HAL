//
// Includes
//

// stdlib
#include <stddef.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

// FreeRTOS
#include <FreeRTOS.h>



//
// Implementation
//

// On the host the C library keeps its own allocator: the FreeRTOS heap is far too small
// for hosted code (the test runner alone needs more) and would make every malloc() fail.
#if !defined(_MSC_VER) && !defined(UNI_HAL_TARGET_MCU_PC)
void *calloc(size_t num, size_t size) {
    void *result = NULL;
    if (num > 0U && size > 0U) {
        result = pvPortCalloc(num, size);
    }
    return result;
}

void *malloc(size_t size) {
    void *result = NULL;
    if (size > 0U) {
        result = pvPortMalloc(size);
    }
    return result;
}

void free(void *ptr) {
    if (ptr != NULL) {
        vPortFree(ptr);
    }
}
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

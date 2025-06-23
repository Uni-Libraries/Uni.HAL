//
// Includes
//

// stdlib
#include <stddef.h>
#include <string.h>

// FreeRTOS
#include <FreeRTOS.h>



//
// Implementation
//

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

void vApplicationMallocFailedHook( void )
{
    volatile uint32_t c = 0;
    while (!c) {
        __builtin_trap();
    }
}

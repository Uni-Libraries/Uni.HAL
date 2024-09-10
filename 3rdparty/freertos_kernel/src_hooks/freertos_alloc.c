//
// Includes
//

// stdlib
#include <string.h>

// FreeRTOS
#include <FreeRTOS.h>



//
// Implementation
//

__attribute__((unused)) void *calloc(size_t num, size_t size) {
    void *result = nullptr;
    if (num > 0U && size > 0U) {
        result = pvPortCalloc(num, size);
    }
    return result;
}

void *malloc(size_t size) {
    void *result = nullptr;
    if (size > 0U) {
        result = pvPortMalloc(size);
    }
    return result;
}

void free(void *ptr) {
    if (ptr != nullptr) {
        vPortFree(ptr);
    }
}

#include <errno.h>
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>

#if configAPPLICATION_ALLOCATED_HEAP
alignas(portBYTE_ALIGNMENT) uint8_t ucHeap[configTOTAL_HEAP_SIZE];
#endif

// Inspect this value after main on a board. A failing check records its source line.
volatile int allocator_test_result = -1;
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

#if defined(__PICOLIBC__) && defined(PICOLIBC_TLS)
static thread_local volatile unsigned char tls_initial = 0x51;
alignas(64) static thread_local volatile unsigned char tls_zero;
static bool constructor_saw_tls;

__attribute__((constructor)) static void check_startup_tls(void) {
    constructor_saw_tls = tls_initial == 0x51 && tls_zero == 0;
}
#endif

static int check_allocators(void) {
#if defined(__PICOLIBC__) && defined(PICOLIBC_TLS)
    CHECK(constructor_saw_tls);
    // The first call changes the boot thread's copy. Every new task starts from the template.
    CHECK(tls_initial == 0x51 && tls_zero == 0);
    tls_initial = 0;
    tls_zero = 1;
#endif
    void *kernel_block = pvPortMalloc(16);
    CHECK(kernel_block != nullptr);
    vPortFree(kernel_block);
    // A dynamic-only FreeRTOS port can allocate its libc lock on first use.
    void *warmup = malloc(1);
    CHECK(warmup != nullptr);
    free(warmup);
    size_t kernel_free = xPortGetFreeHeapSize();

    unsigned char *block = malloc(19);
    CHECK(block != nullptr);
    memset(block, 0x51, 19);
    unsigned char *grown = realloc(block, 113);
    CHECK(grown != nullptr && grown[0] == 0x51 && grown[18] == 0x51);
    unsigned char *shrunk = realloc(grown, 8);
    CHECK(shrunk != nullptr && shrunk[0] == 0x51 && shrunk[7] == 0x51);
    errno = 0;
    CHECK(realloc(shrunk, 0x40000000UL) == nullptr);
    CHECK(errno == ENOMEM);
    CHECK(shrunk[7] == 0x51);
    free(shrunk);

    unsigned char *zeroed = calloc(5, 9);
    CHECK(zeroed != nullptr);
    for (size_t i = 0; i < 45; ++i) {
        CHECK(zeroed[i] == 0);
    }

#if defined(__NEWLIB__) && !defined(__PICOLIBC__)
    // Newlib's own clients also exchange pointers with the public API.
    grown = _realloc_r(_REENT, zeroed, 91);
    CHECK(grown != nullptr && grown[44] == 0);
    _free_r(_REENT, grown);
    void *internal = _malloc_r(_REENT, 17);
    CHECK(internal != nullptr);
    free(internal);
    internal = _calloc_r(_REENT, 4, 7);
    CHECK(internal != nullptr);
    free(internal);
    unsigned char *aligned = _memalign_r(_REENT, 64, 71);
    CHECK(aligned != nullptr && (uintptr_t)aligned % 64 == 0);
    free(aligned);
#else
    free(zeroed);
#endif

    void *aligned_block = memalign(32, 65);
    CHECK(aligned_block != nullptr && (uintptr_t)aligned_block % 32 == 0);
    CHECK(xPortGetFreeHeapSize() == kernel_free);
    free(aligned_block);
    return 0;
}

int main(void) {
    allocator_test_result = check_allocators();
    return allocator_test_result;
}

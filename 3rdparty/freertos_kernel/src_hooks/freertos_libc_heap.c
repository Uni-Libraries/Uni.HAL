#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(UNI_HAL_LIBC_HEAP_SIZE)
static_assert(UNI_HAL_LIBC_HEAP_SIZE > 0);
#if defined(UNI_HAL_TARGET_MCU_STM32H743) || defined(UNI_HAL_TARGET_MCU_STM32H753)
__attribute__((section(".RAM_D1")))
#endif
alignas(max_align_t) static unsigned char libc_heap[UNI_HAL_LIBC_HEAP_SIZE];
static size_t used;

// A firmware with its own C library heap can supply a strong syscall instead.
// The allocator holds its recursive lock while growing or trimming the heap.
#if defined(__PICOLIBC__)
__attribute__((weak)) void *sbrk(ptrdiff_t increment)
#else
__attribute__((weak)) void *_sbrk(ptrdiff_t increment)
#endif
{
    const size_t previous = used;
    // Unsigned subtraction also handles PTRDIFF_MIN without signed overflow.
    const size_t amount = increment < 0 ? 0U - (size_t)increment : (size_t)increment;
    if (amount > (increment < 0 ? used : sizeof(libc_heap) - used)) {
        errno = ENOMEM;
        return (void *)-1;
    }
    used = increment < 0 ? used - amount : used + amount;
    return libc_heap + previous;
}
#endif

#if defined(__NEWLIB__) && !defined(__PICOLIBC__)
#include <reent.h>

extern void *_sbrk(ptrdiff_t increment);

// The Xilinx BSP reports an exhausted linker heap by returning -1 alone.
// Preserve the error in the requesting newlib reentrancy context on all MCUs.
__attribute__((weak)) void *_sbrk_r(struct _reent *ctx, ptrdiff_t increment) {
    void *previous = _sbrk(increment);
    if (previous == (void *)-1) {
        _REENT_ERRNO(ctx) = ENOMEM;
    }
    return previous;
}
#endif

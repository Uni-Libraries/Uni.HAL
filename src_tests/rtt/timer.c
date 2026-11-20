#include "timer.h"

#if configAPPLICATION_ALLOCATED_HEAP
alignas(portBYTE_ALIGNMENT) uint8_t ucHeap[configTOTAL_HEAP_SIZE];
#endif

timer_failure_t fail_at;
unsigned init_calls, stop_calls;
bool running;
uni_hal_tim_callback_fn callback;
void *callback_ctx;

bool __wrap_uni_hal_tim_init(uni_hal_tim_context_t *ctx) {
    ++init_calls;
    return ctx && fail_at != INIT_FAILURE;
}

bool __wrap_uni_hal_tim_register_callback(uni_hal_tim_context_t *ctx, uni_hal_tim_callback_fn fn, void *user) {
    if (!ctx || (fn && fail_at == REGISTER_FAILURE)) return false;
    callback = fn;
    callback_ctx = user;
    return true;
}

bool __wrap_uni_hal_tim_start(uni_hal_tim_context_t *ctx) {
    running = ctx && callback;
    return running && fail_at != START_FAILURE;
}

bool __wrap_uni_hal_tim_stop(uni_hal_tim_context_t *ctx) {
    ++stop_calls;
    running = false;
    return ctx != nullptr;
}

void finish(unsigned code) {
    const uint32_t result[] = {0x20026U, code};
    register uint32_t operation __asm__("r0") = 0x20U;
    register const void *args __asm__("r1") = result;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

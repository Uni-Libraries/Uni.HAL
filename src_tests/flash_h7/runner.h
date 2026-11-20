#pragma once
#include <stdint.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
static unsigned check(void);
extern char _stack_top;
void run_tests(void);
__attribute__((used, section(".test_vectors")))
const uintptr_t vectors[] = {(uintptr_t)&_stack_top, (uintptr_t)run_tests};
void run_tests(void) {
    uint32_t result[] = {0x20026U, check()};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = result;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

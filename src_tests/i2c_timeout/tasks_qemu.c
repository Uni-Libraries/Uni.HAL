// The Cortex-M4 scheduler runs both tasks. The H743 I2C handles and pins are modeled.
#include "i2c/uni_hal_i2c.h"

bool __wrap_uni_hal_rcc_clk_set(uni_hal_core_periph_e target, bool enabled) {
    (void)target;
    return enabled;
}

bool __wrap_uni_hal_rcc_clksrc_set(uni_hal_core_periph_e target, uni_hal_rcc_clksrc_e source) {
    (void)target;
    (void)source;
    return true;
}

_Noreturn void finish(unsigned code) {
    uint32_t result[] = {0x20026U, code};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = result;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

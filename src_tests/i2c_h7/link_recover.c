#include "i2c/uni_hal_i2c.h"

// Keep the public recovery entry point and its HAL/LL dependencies in the final firmware.
static bool (*volatile recover)(uni_hal_i2c_context_t *) = uni_hal_i2c_recover;

int main(void) {
    return recover == nullptr;
}

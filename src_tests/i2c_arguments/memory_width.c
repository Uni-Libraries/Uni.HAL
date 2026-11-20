#include "register_model.h"

static unsigned check(void) {
    uni_hal_i2c_context_t ctx = {};
    CHECK(init_bus(&ctx));

    uint8_t byte = 0xAB;
    for (unsigned read = 0; read < 2; ++read) {
        uni_hal_i2c_memadd_size_e const widths[] = {
            (uni_hal_i2c_memadd_size_e)2, UNI_HAL_I2C_MEMADD_SIZE_8BIT, UNI_HAL_I2C_MEMADD_SIZE_16BIT,
        };
        for (unsigned i = 0; i < 3; ++i) {
            starts = 0;
            bool ok = read ? uni_hal_i2c_mem_read(&ctx, 0x50, 0x1234, widths[i], &byte, 1, 20)
                           : uni_hal_i2c_mem_write(&ctx, 0x50, 0x1234, widths[i], &byte, 1, 20);
            CHECK(!ok && byte == 0xAB);
            if (i == 0) {
                CHECK(starts == 0);
            } else {
                // For valid widths the addressed device NACKs the register phase.
                CHECK(starts == 1 && ((last_start & I2C_CR2_NBYTES) >> I2C_CR2_NBYTES_Pos) == i);
            }
        }
    }
    return 0;
}

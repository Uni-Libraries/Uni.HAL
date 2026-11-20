#include "register_model.h"

static unsigned check(void) {
    uni_hal_i2c_context_t ctx = {};
    CHECK(init_bus(&ctx));

    responding_address = 0x40;
    uint8_t byte = 0x5A;
    uint16_t const invalid[] = {0x0480U, 0x0100U, UINT16_MAX};
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        uint16_t const addr = invalid[i];
        // ST masks 0x0480 to 0x80 in CR2, the address byte for a device at 0x40.
        CHECK(!uni_hal_i2c_isready(&ctx, addr, 1, 20));
        CHECK(!uni_hal_i2c_master_receive(&ctx, addr, &byte, 1, 20));
        CHECK(!uni_hal_i2c_master_transmit(&ctx, addr, &byte, 1, 20));
        CHECK(!uni_hal_i2c_mem_read(&ctx, addr, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, &byte, 1, 20));
        CHECK(!uni_hal_i2c_mem_write(&ctx, addr, 7, UNI_HAL_I2C_MEMADD_SIZE_8BIT, &byte, 1, 20));
        CHECK(starts == 0 && byte == 0x5A);
    }

    static const struct {
        uint16_t argument;
        uint8_t address;
    } valid[] = {{0x00, 0x00}, {0x40, 0x40}, {0x7F, 0x7F}, {0x80, 0x40}, {0xA0, 0x50}, {0xFF, 0x7F}};
    for (unsigned i = 0; i < sizeof valid / sizeof valid[0]; ++i) {
        responding_address = valid[i].address;
        CHECK(uni_hal_i2c_isready(&ctx, valid[i].argument, 1, 20));
        CHECK(starts == i + 1U);
        CHECK(((last_start & 0xFEU) >> 1U) == valid[i].address);
    }
    return 0;
}

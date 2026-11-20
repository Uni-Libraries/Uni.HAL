#include <catch2/catch_test_macros.hpp>

#include "i2c/uni_hal_i2c.h"
#include "periph/uni_hal_periph_ads1015.h"
#include "periph/uni_hal_periph_cy15b064j.h"
#include "systick/uni_hal_systick.h"

namespace {
uint32_t now;
}

// Supply device latency without sleeping, including across the 32-bit wrap.
extern "C" uint32_t uni_hal_systick_get_ms(void) {
    return now;
}

TEST_CASE("PC I2C reports no devices on an empty bus", "[hal_i2c]") {
    // A board configuration that specifies every hardware field also describes an empty host bus.
    constexpr uni_hal_i2c_config_t config{
        .instance = UNI_HAL_CORE_PERIPH_I2C_1, .clock_source = UNI_HAL_RCC_CLKSRC_UNKNOWN,
        .speed = UNI_HAL_I2C_SPEED_100KHZ, .timing = 0, .pin_sck = nullptr, .pin_sda = nullptr,
        .irq_enable = false, .irq_priority = 0,
    };
    static_assert(config.pc_exchange == nullptr && config.pc_user == nullptr);
    uni_hal_i2c_context_t bus{.config = config, .state = {}};
    REQUIRE(uni_hal_i2c_init(&bus));
    uint8_t bytes[] = {0xA5, 0xBC};
    REQUIRE_FALSE(uni_hal_i2c_isready(&bus, 0x48, 3, 10));
    REQUIRE_FALSE(uni_hal_i2c_master_transmit(&bus, 0x48, bytes, 2, 10));
    REQUIRE_FALSE(uni_hal_i2c_master_receive(&bus, 0x48, bytes, 2, 10));
    REQUIRE_FALSE(uni_hal_i2c_mem_write(&bus, 0x48, 1, UNI_HAL_I2C_MEMADD_SIZE_8BIT, bytes, 2, 10));
    REQUIRE_FALSE(uni_hal_i2c_mem_read(&bus, 0x48, 1, UNI_HAL_I2C_MEMADD_SIZE_8BIT, bytes, 2, 10));
    REQUIRE(bytes[0] == 0xA5);
    REQUIRE(bytes[1] == 0xBC);
    uni_hal_ads1015_context_t sensor{};
    sensor.config.i2c = &bus;
    sensor.config.address = 0x48;
    sensor.config.timeout = 10;
    REQUIRE_FALSE(uni_hal_ads1015_init(&sensor));
    REQUIRE_FALSE(uni_hal_ads1015_is_inited(&sensor));
}

namespace {
struct Device {
    uint8_t data[2]{};
    unsigned calls{};
    uint32_t duration{};
    int32_t transfer_len = 2;
};

int32_t exchange(void *user, const uni_hal_i2c_pc_request_t *request) {
    auto &device = *static_cast<Device *>(user);
    ++device.calls;
    now += device.duration;
    REQUIRE(request->timeout == 1000);
    if (request->dev_addr != 0x50) {
        return -1;
    }
    if (request->operation == UNI_HAL_I2C_PC_PROBE) {
        REQUIRE(request->trials == 3);
        return 0;
    }
    REQUIRE(request->buf_len == 2);
    if (request->operation == UNI_HAL_I2C_PC_MEM_READ || request->operation == UNI_HAL_I2C_PC_MEM_WRITE) {
        REQUIRE(request->mem_addr == 0x1ABC);
        REQUIRE(request->mem_addr_size == UNI_HAL_I2C_MEMADD_SIZE_16BIT);
    }
    for (int32_t i = 0; i < device.transfer_len; ++i) {
        if (request->operation == UNI_HAL_I2C_PC_MEM_READ || request->operation == UNI_HAL_I2C_PC_MASTER_RECEIVE) {
            request->buf[i] = device.data[i];
        } else {
            device.data[i] = request->buf[i];
        }
    }
    return device.transfer_len;
}
}

TEST_CASE("PC I2C forwards complete transactions to the addressed bus model", "[hal_i2c]") {
    Device first{}, second{};
    uni_hal_i2c_context_t bus{};
    bus.config.pc_exchange = exchange;
    bus.config.pc_user = &first;
    uni_hal_i2c_context_t other = bus;
    other.config.pc_user = &second;
    REQUIRE(uni_hal_i2c_init(&other));
    REQUIRE_FALSE(uni_hal_i2c_isready(&bus, 0x50, 3, 1000));
    REQUIRE(uni_hal_i2c_init(&bus));
    REQUIRE_FALSE(uni_hal_i2c_isready(&bus, 0x180, 3, 1000));
    REQUIRE_FALSE(uni_hal_i2c_isready(&bus, 0x50, 0, 1000));
    REQUIRE_FALSE(uni_hal_i2c_isready(&bus, 0x50, 3, 0));
    REQUIRE_FALSE(uni_hal_i2c_master_receive(&bus, 0x50, nullptr, 2, 1000));
    REQUIRE(first.calls == 0);
    REQUIRE(uni_hal_i2c_isready(&bus, 0x50, 3, 1000));
    REQUIRE_FALSE(uni_hal_i2c_isready(&bus, 0x51, 3, 1000));

    uint8_t tx[] = {0x23, 0x84}, rx[] = {0xFF, 0xFF};
    REQUIRE(uni_hal_i2c_master_transmit(&bus, 0xA0, tx, 2, 1000));
    REQUIRE(uni_hal_i2c_master_receive(&bus, 0x50, rx, 2, 1000));
    REQUIRE(rx[0] == tx[0]);
    REQUIRE(rx[1] == tx[1]);
    REQUIRE(uni_hal_i2c_master_receive(&other, 0x50, rx, 2, 1000));
    REQUIRE(rx[0] == 0);
    REQUIRE(rx[1] == 0);

    uni_hal_cy15b064j_context_t fram{};
    fram.config.i2c = &bus;
    fram.config.address = 0x50;
    fram.config.timeout = 1000;
    REQUIRE(uni_hal_cy15b064j_init(&fram));
    tx[0] = 0xE7;
    REQUIRE(uni_hal_cy15b064j_write(&fram, 0x1ABC, 2, tx));
    REQUIRE(uni_hal_cy15b064j_read(&fram, 0x1ABC, 2, rx));
    REQUIRE(rx[0] == tx[0]);
    REQUIRE(rx[1] == tx[1]);
    first.transfer_len = 1;
    REQUIRE_FALSE(uni_hal_cy15b064j_read(&fram, 0x1ABC, 2, rx));
    REQUIRE_FALSE(uni_hal_cy15b064j_write(&fram, 0x1ABC, 2, tx));

    REQUIRE(uni_hal_i2c_deinit(&bus));
    auto const calls_before = first.calls;
    REQUIRE_FALSE(uni_hal_i2c_master_receive(&bus, 0xA0, rx, 2, 1000));
    REQUIRE(first.calls == calls_before);
    REQUIRE(uni_hal_i2c_reset(&bus));
    now = UINT32_MAX - 500;
    first.duration = 1000;
    REQUIRE(uni_hal_i2c_isready(&bus, 0x50, 3, 1000));
    first.duration = 1001;
    REQUIRE_FALSE(uni_hal_i2c_isready(&bus, 0x50, 3, 1000));
}

TEST_CASE("ADS1015 receives its configured conversion through the PC I2C driver", "[hal_i2c]") {
    uni_hal_i2c_context_t bus{};
    bus.config.pc_exchange = [](void *, const uni_hal_i2c_pc_request_t *request) -> int32_t {
        REQUIRE(request->dev_addr == 0x48);
        REQUIRE(request->mem_addr_size == UNI_HAL_I2C_MEMADD_SIZE_8BIT);
        REQUIRE(request->buf_len == 2);
        if (request->operation == UNI_HAL_I2C_PC_MEM_WRITE && request->mem_addr == UNI_HAL_ADS1015_REG_CONFIG) {
            REQUIRE(request->buf[0] == 0x50);
            REQUIRE(request->buf[1] == 0);
            return 2;
        }
        if (request->operation == UNI_HAL_I2C_PC_MEM_READ && request->mem_addr == UNI_HAL_ADS1015_REG_CONVERSION) {
            request->buf[0] = 0xFE;
            request->buf[1] = 0xC0;
            return 2;
        }
        return -1;
    };
    uni_hal_ads1015_context_t sensor{};
    sensor.config.i2c = &bus;
    sensor.config.address = 0x48;
    sensor.config.mode = UNI_HAL_ADS1015_MODE_CONT;
    sensor.config.mux = UNI_HAL_ADS1015_MUX_IN1_GND;
    sensor.config.timeout = 1000;
    REQUIRE(uni_hal_ads1015_init(&sensor));
    REQUIRE(uni_hal_ads1015_get_raw(&sensor) == -20);
}

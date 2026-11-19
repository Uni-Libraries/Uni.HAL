#include <catch2/catch_test_macros.hpp>
#include <initializer_list>
#include <span>

#include "periph/uni_hal_periph_ads1015.h"
#include "systick/uni_hal_systick.h"

namespace {
enum class Operation { start, status, sample };

struct Exchange {
    Operation operation;
    uint32_t timeout;
    uint32_t duration;
    uint16_t value;
    bool success = true;
};

std::span<const Exchange> pending;
uint32_t now;
uni_hal_i2c_context_t bus{};

Exchange next_exchange(Operation operation, uint32_t timeout) {
    REQUIRE_FALSE(pending.empty());
    Exchange step = pending.front();
    REQUIRE(step.operation == operation);
    REQUIRE(step.timeout == timeout);
    pending = pending.subspan(1);
    now += step.duration;
    return step;
}

void check_sample(uni_hal_ads1015_mode_e mode, uint32_t timeout, std::initializer_list<Exchange> script,
                  int16_t expected, uint32_t start = 0U) {
    uni_hal_ads1015_context_t sensor{};
    sensor.config.i2c = &bus;
    sensor.config.address = 0x48U;
    sensor.config.mode = mode;
    sensor.config.timeout = timeout;
    sensor.state.initialized = true;
    now = start;
    pending = {script.begin(), script.size()};
    REQUIRE(uni_hal_ads1015_get_raw(&sensor) == expected);
    REQUIRE(pending.empty());
}
} // namespace

extern "C" uint32_t uni_hal_systick_get_ms(void) {
    return now;
}

extern "C" bool uni_hal_i2c_init(uni_hal_i2c_context_t *ctx) {
    return ctx == &bus;
}

extern "C" bool uni_hal_i2c_is_inited(uni_hal_i2c_context_t *ctx) {
    return ctx == &bus;
}

extern "C" bool uni_hal_i2c_mem_write(uni_hal_i2c_context_t *ctx, uint16_t address, uint16_t reg,
                                      uni_hal_i2c_memadd_size_e reg_size, uint8_t *data, uint16_t len,
                                      uint32_t timeout) {
    REQUIRE(ctx == &bus);
    REQUIRE(address == 0x48U);
    REQUIRE(reg == UNI_HAL_ADS1015_REG_CONFIG);
    REQUIRE(reg_size == UNI_HAL_I2C_MEMADD_SIZE_8BIT);
    REQUIRE(len == 2U);
    Exchange step = next_exchange(Operation::start, timeout);
    REQUIRE(((data[0] << 8) | data[1]) == step.value);
    return step.success;
}

extern "C" bool uni_hal_i2c_mem_read(uni_hal_i2c_context_t *ctx, uint16_t address, uint16_t reg,
                                     uni_hal_i2c_memadd_size_e reg_size, uint8_t *data, uint16_t len,
                                     uint32_t timeout) {
    REQUIRE(ctx == &bus);
    REQUIRE(address == 0x48U);
    REQUIRE(reg_size == UNI_HAL_I2C_MEMADD_SIZE_8BIT);
    REQUIRE(len == 2U);
    REQUIRE((reg == UNI_HAL_ADS1015_REG_CONFIG || reg == UNI_HAL_ADS1015_REG_CONVERSION));
    Exchange step = next_exchange(reg == UNI_HAL_ADS1015_REG_CONFIG ? Operation::status : Operation::sample, timeout);
    if (step.success) {
        data[0] = static_cast<uint8_t>(step.value >> 8);
        data[1] = static_cast<uint8_t>(step.value);
    }
    return step.success;
}

TEST_CASE("ADS1015 stops on a failed start, status or sample read", "[ads1015]") {
    check_sample(UNI_HAL_ADS1015_MODE_SINGLE, 10, {{Operation::start, 10, 1, 0x8100, false}}, INT16_MAX);
    check_sample(UNI_HAL_ADS1015_MODE_SINGLE, 10, {
        {Operation::start, 10, 1, 0x8100}, {Operation::status, 9, 1, 0, false}
    }, INT16_MAX);
    check_sample(UNI_HAL_ADS1015_MODE_CONT, 10, {{Operation::sample, 10, 1, 0, false}}, INT16_MAX);
}

TEST_CASE("ADS1015 shares one deadline across single-shot conversion and readout", "[ads1015]") {
    check_sample(UNI_HAL_ADS1015_MODE_SINGLE, 5, {
        {Operation::start, 5, 1, 0x8100}, {Operation::status, 4, 2, 0}, {Operation::status, 2, 2, 0}
    }, INT16_MAX);
    check_sample(UNI_HAL_ADS1015_MODE_SINGLE, 3, {
        {Operation::start, 3, 1, 0x8100}, {Operation::status, 2, 2, 0x8000}
    }, INT16_MAX);
    check_sample(UNI_HAL_ADS1015_MODE_SINGLE, 0, {}, INT16_MAX);
}

TEST_CASE("ADS1015 reads signed samples at the deadline and across clock wrap", "[ads1015]") {
    check_sample(UNI_HAL_ADS1015_MODE_SINGLE, 8, {
        {Operation::start, 8, 2, 0x8100}, {Operation::status, 6, 2, 0},
        {Operation::status, 4, 2, 0x8000}, {Operation::sample, 2, 2, 0xfff0}
    }, -1, UINT32_MAX - 2U);
    check_sample(UNI_HAL_ADS1015_MODE_CONT, 5, {{Operation::sample, 5, 5, 0x1230}}, 291);
    check_sample(UNI_HAL_ADS1015_MODE_CONT, 5, {{Operation::sample, 5, 6, 0x1230}}, INT16_MAX);
}

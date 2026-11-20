#include <catch2/catch_test_macros.hpp>
#include "crc/uni_hal_crc.h"

namespace {
constexpr uint8_t digits[] = "123456789";
constexpr uint32_t digit_count = sizeof(digits) - 1;

uni_hal_crc_context_t config(uni_hal_crc_polysize_e width, uint32_t poly, uint32_t seed) {
    return {UNI_HAL_CORE_PERIPH_CRC_1, seed, poly, width, false};
}
}

TEST_CASE("PC CRC matches the four STM32 widths for complete and split messages") {
    // Check values for "123456789" from the CRC RevEng catalogue:
    // https://reveng.sourceforge.io/crc-catalogue/all.htm
    struct Vector {
        uni_hal_crc_polysize_e width;
        uint32_t polynomial, seed, check;
    };
    constexpr Vector vectors[] = {
        {UNI_HAL_CRC_POLYSIZE_7B,  0x09,       0,          0x75},       // CRC-7/MMC
        {UNI_HAL_CRC_POLYSIZE_8B,  0x07,       0,          0xF4},       // CRC-8/SMBUS
        {UNI_HAL_CRC_POLYSIZE_16B, 0x1021,     0xFFFF,     0x29B1},     // CRC-16/IBM-3740
        {UNI_HAL_CRC_POLYSIZE_32B, 0x04C11DB7, UINT32_MAX, 0x0376E6E7} // CRC-32/MPEG-2
    };
    for (auto v : vectors) {
        auto ctx = config(v.width, v.polynomial, v.seed);
        REQUIRE(uni_hal_crc_init(&ctx));
        REQUIRE(uni_hal_crc_calculate(&ctx, digits, digit_count) == v.check);
        for (uint32_t split = 0; split <= digit_count; ++split) {
            REQUIRE(uni_hal_crc_reset(&ctx));
            REQUIRE(uni_hal_crc_get_value(&ctx) == v.seed);
            REQUIRE(uni_hal_crc_append(&ctx, digits, split));
            REQUIRE(uni_hal_crc_append(&ctx, digits + split, digit_count - split));
            REQUIRE(uni_hal_crc_get_value(&ctx) == v.check);
        }
    }
}

TEST_CASE("PC CRC supports independent streams, custom seeds and alternate polynomials") {
    auto ccitt = config(UNI_HAL_CRC_POLYSIZE_16B, 0x1021, 0xFFFF);
    auto umts = config(UNI_HAL_CRC_POLYSIZE_16B, 0x8005, 0);
    REQUIRE(uni_hal_crc_init(&ccitt));
    REQUIRE(uni_hal_crc_init(&umts));
    REQUIRE(uni_hal_crc_append(&ccitt, digits, 2));
    REQUIRE(uni_hal_crc_calculate(&umts, digits, digit_count) == 0xFEE8); // CRC-16/UMTS
    REQUIRE(uni_hal_crc_append(&ccitt, digits + 2, digit_count - 2));
    REQUIRE(uni_hal_crc_get_value(&ccitt) == 0x29B1);
    REQUIRE(uni_hal_crc_get_value(&umts) == 0xFEE8);

    REQUIRE(uni_hal_crc_calculate_ex(&ccitt, 0, digits, digit_count) == 0x31C3); // CRC-16/XMODEM
    REQUIRE(uni_hal_crc_reset_ex(&ccitt, 0xABCD1234));
    REQUIRE(uni_hal_crc_get_value(&ccitt) == 0x1234);
    REQUIRE(uni_hal_crc_calculate(&ccitt, digits, 0) == 0xFFFF);

    // Four all-ones bytes cancel the all-ones seed before any feedback.
    auto mpeg2 = config(UNI_HAL_CRC_POLYSIZE_32B, 0x04C11DB7, UINT32_MAX);
    constexpr uint8_t all_ones[] = {0xFF, 0xFF, 0xFF, 0xFF};
    REQUIRE(uni_hal_crc_init(&mpeg2));
    REQUIRE(uni_hal_crc_calculate(&mpeg2, all_ones, sizeof(all_ones)) == 0);
}

TEST_CASE("PC CRC rejects invalid configurations and data without advancing the stream") {
    auto ctx = config(UNI_HAL_CRC_POLYSIZE_8B, 0x07, 0x42);
    REQUIRE_FALSE(uni_hal_crc_init(nullptr));
    REQUIRE_FALSE(uni_hal_crc_append(&ctx, digits, digit_count));
    REQUIRE_FALSE(uni_hal_crc_reset(&ctx));
    REQUIRE(uni_hal_crc_init(&ctx));
    REQUIRE_FALSE(uni_hal_crc_append(&ctx, nullptr, 1));
    REQUIRE(uni_hal_crc_calculate_ex(&ctx, 0, nullptr, 1) == 0);
    REQUIRE(uni_hal_crc_get_value(&ctx) == 0x42);

    ctx.polynomial_coef = 0x06; // STM32 requires the constant term.
    REQUIRE_FALSE(uni_hal_crc_init(&ctx));
    REQUIRE_FALSE(uni_hal_crc_is_inited(&ctx));
    ctx.polynomial_coef = 0x07;
    ctx.polynomial_size = static_cast<uni_hal_crc_polysize_e>(9);
    REQUIRE_FALSE(uni_hal_crc_init(&ctx));
}

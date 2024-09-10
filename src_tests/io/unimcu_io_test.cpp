//
// Includes
//

// stdlib

#include <cstring>

// catch2
#include <catch2/catch_test_macros.hpp>

// uni.hal
#include <uni_common.h>
#include <uni_hal.h>


//
// tests
//

TEST_CASE("io_init", "[hal_io]") {
    SECTION("nullptr") { REQUIRE_FALSE(uni_hal_io_init(nullptr)); }

    SECTION("ok-1") {
        UNI_HAL_IO_DEFINITION(io, 128, 128);
        REQUIRE(uni_hal_io_init(&io_ctx));
    }
}


TEST_CASE("io_clear", "[hal_io]") {
    SECTION("nullptr") { REQUIRE_FALSE(uni_hal_io_receive_clear(nullptr)); }

    SECTION("ok-1") {
        UNI_HAL_IO_DEFINITION(io, 128, 128);
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == 0);

        const char *str_1 = "ABCDEF12345\r\n";
        REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, (const uint8_t *)str_1, strlen(str_1)) == strlen(str_1));

        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_clear(&io_ctx));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == 0U);
    }
}


TEST_CASE("io_read_data", "[hal_io]") {
    SECTION("nullptr") {
        REQUIRE_FALSE(uni_hal_io_receive_available(nullptr));
    }

    SECTION("ok-1") {
        UNI_HAL_IO_DEFINITION(io, 128, 128);

        REQUIRE(uni_hal_io_receive_available(&io_ctx) == 0U);

        uint8_t t = '1';
        uint8_t t_2 = '0';
        REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, &t, 1U));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == 1U);
        REQUIRE(uni_hal_io_receive_data(&io_ctx, &t_2, sizeof(t_2), 0));
        REQUIRE(t == t_2);
        REQUIRE_FALSE(uni_hal_io_receive_data(&io_ctx, &t_2, sizeof(t_2), 0));
    }
}


TEST_CASE("io_read_line", "[hal_io]") {
    UNI_HAL_IO_DEFINITION(io, 128, 128);
    REQUIRE(uni_hal_io_receive_available(&io_ctx) == 0);

    const char *str_1 = "ABCDEF12345\r\n";
    const char *str_2 = "IIIIII67890AB\r\n";

    REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, (const uint8_t *)str_1, strlen(str_1)) == strlen(str_1));
    REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));

    REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, (const uint8_t *)str_2, strlen(str_2)) == strlen(str_2));
    REQUIRE(uni_hal_io_receive_available(&io_ctx) == (strlen(str_1) + strlen(str_2)));


    char str_1_rb[128]{};
    size_t str_1_rb_len = uni_hal_io_receive_line(&io_ctx, (uint8_t *)str_1_rb, sizeof(str_1_rb), 0);
    REQUIRE(strlen(str_1_rb) == str_1_rb_len);
    REQUIRE(strlen(str_1) == str_1_rb_len);
    REQUIRE(strcmp(str_1_rb, str_1) == 0);
    REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_2));

    char str_2_rb[128]{};
    size_t str_2_rb_len = uni_hal_io_receive_line(&io_ctx, (uint8_t *)str_2_rb, sizeof(str_2_rb), 0);
    REQUIRE(strlen(str_2_rb) == str_2_rb_len);
    REQUIRE(strlen(str_2) == str_2_rb_len);
    REQUIRE(strcmp(str_2_rb, str_2) == 0);

    REQUIRE(uni_hal_io_receive_available(&io_ctx) == 0U);
}


TEST_CASE("io_read_sync", "[hal_io]") {
    UNI_HAL_IO_DEFINITION(io, 128, 128);

    SECTION("nullptr"){
        REQUIRE_FALSE(uni_hal_io_receive_sync(nullptr,reinterpret_cast<const uint8_t*>("ABC"), strlen("ABC"), 0));
        REQUIRE_FALSE(uni_hal_io_receive_sync(&io_ctx,nullptr, strlen("ABC"), 0));
    }

    SECTION("zerolength"){
        const char *str_1 = "ABCDEF12345";
        REQUIRE(uni_hal_io_receive_clear(&io_ctx));
        REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, (const uint8_t *)str_1, strlen(str_1)) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));
        REQUIRE_FALSE(uni_hal_io_receive_sync(&io_ctx,reinterpret_cast<const uint8_t*>("ABCD"), 0, 0));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));
    }

    SECTION("pass") {
        const char *str_1 = "ABCDEF12345";
        REQUIRE(uni_hal_io_receive_clear(&io_ctx));
        REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, (const uint8_t *)str_1, strlen(str_1)) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_sync(&io_ctx,reinterpret_cast<const uint8_t*>("CD"), strlen("CD"), 0));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1) - strlen("AB"));

        char str_2[4]{};
        REQUIRE(uni_hal_io_receive_data(&io_ctx, reinterpret_cast<uint8_t*>(&str_2), sizeof(str_2), 0));
        REQUIRE(memcmp(&str_2, "CDEF", sizeof(uint8_t)*2) == 0);
    }

    SECTION("pass_2") {
        const char *str_1 = "ZAABCDEF";
        REQUIRE(uni_hal_io_receive_clear(&io_ctx));
        REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, (const uint8_t *)str_1, strlen(str_1)) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_sync(&io_ctx, reinterpret_cast<const uint8_t *>("ABC"), strlen("ABC"), 0));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1) - strlen("ZA"));

        char str_2[3]{};
        REQUIRE(uni_hal_io_receive_data(&io_ctx, reinterpret_cast<uint8_t *>(&str_2), sizeof(str_2), 0));
        REQUIRE(memcmp(&str_2, "ABC", sizeof(uint8_t) * 2) == 0);
    }

    SECTION("pass_begin") {
        const char *str_1 = "ABCDEF12345";
        REQUIRE(uni_hal_io_receive_clear(&io_ctx));
        REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, (const uint8_t *)str_1, strlen(str_1)) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_sync(&io_ctx,reinterpret_cast<const uint8_t*>("ABC"), strlen("ABC"), 0));
        char str_2[4]{};
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));

        REQUIRE(uni_hal_io_receive_data(&io_ctx, reinterpret_cast<uint8_t*>(&str_2), sizeof(str_2), 0));
        REQUIRE(memcmp(&str_2, "ABCD", sizeof(uint8_t)*2) == 0);

    }

    SECTION("pass_end") {
        const char *str_1 = "ABCDEF12345";
        REQUIRE(uni_hal_io_receive_clear(&io_ctx));
        REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, (const uint8_t *)str_1, strlen(str_1)) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_sync(&io_ctx,reinterpret_cast<const uint8_t*>("345"), strlen("345"), 0));
        char str_2[3]{};
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen("345"));

        REQUIRE(uni_hal_io_receive_data(&io_ctx, reinterpret_cast<uint8_t*>(&str_2), sizeof(str_2), 0));
        REQUIRE(memcmp(&str_2, "345", sizeof(uint8_t)*2) == 0);

    }

    SECTION("fail_partial"){
        const char *str_1 = "ABCDEF12345";
        REQUIRE(uni_hal_io_receive_clear(&io_ctx));
        REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, (const uint8_t *)str_1, strlen(str_1)) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));
        REQUIRE_FALSE(uni_hal_io_receive_sync(&io_ctx,reinterpret_cast<const uint8_t*>("456"), strlen("456"), 0));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen("456")-1);
    }

    SECTION("fail_distance"){
        const char *str_1 = "ABCDEF12345";
        REQUIRE(uni_hal_io_receive_clear(&io_ctx));
        REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, (const uint8_t *)str_1, strlen(str_1)) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));
        REQUIRE_FALSE(uni_hal_io_receive_sync(&io_ctx,reinterpret_cast<const uint8_t*>("DF"), strlen("DF"), 0));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == 0U);
    }

    SECTION("fail_full"){
        const char *str_1 = "ABCDEF12345";
        REQUIRE(uni_hal_io_receive_clear(&io_ctx));
        REQUIRE(uni_hal_ringbuffer_push(&io_rb_rx_ctx, (const uint8_t *)str_1, strlen(str_1)) == strlen(str_1));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == strlen(str_1));
        REQUIRE_FALSE(uni_hal_io_receive_sync(&io_ctx,reinterpret_cast<const uint8_t*>("99"), strlen("99"), 0));
        REQUIRE(uni_hal_io_receive_available(&io_ctx) == 0U);
    }
}

TEST_CASE("io_transmit_data", "[hal_io]") {
    SECTION("nullptr") { REQUIRE_FALSE(uni_hal_io_transmit_data(nullptr, nullptr, 10)); }

    SECTION("ok-1") {
        UNI_HAL_IO_DEFINITION(io, 128, 128);

        const char *str_1 = "ABCDEF12345\r\n";

        REQUIRE(uni_hal_io_transmit_data(&io_ctx, (uint8_t *)str_1, strlen(str_1)));
        REQUIRE(uni_hal_ringbuffer_length(&io_rb_tx_ctx) == strlen(str_1));
    }
}

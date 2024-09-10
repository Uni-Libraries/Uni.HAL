//
// Includes
//

// stdlib
#include <cstring>

// catch2
#include <catch2/catch_test_macros.hpp>

// uni_hal
#include <uni_common.h>



//
// typedefs
//

typedef struct {
    size_t a;
    size_t b;
} ringbuffer_item;



//
// defines
//

#define ringbuffer_len (6)
#define ringbuffer_size (ringbuffer_len * sizeof(ringbuffer_item))



//
// globals
//

uint8_t ringbuffer_data[ringbuffer_size];
uni_hal_ringbuffer_context_t ringbuffer_ctx{};



//
// helpers
//

static bool rb_init() {
    memset(ringbuffer_data, 0, ringbuffer_size);
    return uni_hal_ringbuffer_init(&ringbuffer_ctx, ringbuffer_data, sizeof(ringbuffer_item), sizeof(ringbuffer_data));
}


//
// tests
//

TEST_CASE("ringbuffer_init", "[ringbuffer]") {
    REQUIRE(rb_init() == true);
    REQUIRE(uni_hal_ringbuffer_is_empty(&ringbuffer_ctx));
}


TEST_CASE("ringbuffer_push", "[ringbuffer]") {
    rb_init();

    REQUIRE(uni_hal_ringbuffer_is_empty(&ringbuffer_ctx));
    REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 0);

    ringbuffer_item item = {.a = 1, .b = 2};

    REQUIRE(uni_hal_ringbuffer_push(&ringbuffer_ctx, (uint8_t *)&item, 1U));
    REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 1);
    REQUIRE_FALSE(uni_hal_ringbuffer_is_empty(&ringbuffer_ctx));
}


TEST_CASE("ringbuffer_find", "[ringbuffer]") {
    rb_init();

    ringbuffer_item item_a = {.a = 1, .b = 2};
    ringbuffer_item item_b = {.a = 3, .b = 4};

    // initial state
    REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 0);
    REQUIRE(uni_hal_ringbuffer_find(&ringbuffer_ctx, (uint8_t *)&item_a) == SIZE_MAX);
    REQUIRE(uni_hal_ringbuffer_find(&ringbuffer_ctx, (uint8_t *)&item_b) == SIZE_MAX);

    // push item a
    REQUIRE(uni_hal_ringbuffer_push(&ringbuffer_ctx, (uint8_t *)&item_a, 1U));
    REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 1);

    // check items
    REQUIRE(uni_hal_ringbuffer_find(&ringbuffer_ctx, (uint8_t *)&item_a) == 0U);
    REQUIRE(uni_hal_ringbuffer_find(&ringbuffer_ctx, (uint8_t *)&item_b) == SIZE_MAX);

    // push item a once again
    REQUIRE(uni_hal_ringbuffer_push(&ringbuffer_ctx, (uint8_t *)&item_a, 1U));
    REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 2);

    // check items
    REQUIRE(uni_hal_ringbuffer_find(&ringbuffer_ctx, (uint8_t *)&item_a) == 0U);
    REQUIRE(uni_hal_ringbuffer_find(&ringbuffer_ctx, (uint8_t *)&item_b) == SIZE_MAX);

    // push item b
    REQUIRE(uni_hal_ringbuffer_push(&ringbuffer_ctx, (uint8_t *)&item_b, 1U));
    REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 3);

    // check items
    REQUIRE(uni_hal_ringbuffer_find(&ringbuffer_ctx, (uint8_t *)&item_a) == 0U);
    REQUIRE(uni_hal_ringbuffer_find(&ringbuffer_ctx, (uint8_t *)&item_b) == 2U);

    // clear
    REQUIRE(uni_hal_ringbuffer_clear(&ringbuffer_ctx));

    // check
    REQUIRE(uni_hal_ringbuffer_find(&ringbuffer_ctx, (uint8_t *)&item_a) == SIZE_MAX);
    REQUIRE(uni_hal_ringbuffer_find(&ringbuffer_ctx, (uint8_t *)&item_b) == SIZE_MAX);
}


TEST_CASE("ringbuffer_pop", "[ringbuffer]") {
    SECTION("ok-1") {
        rb_init();

        ringbuffer_item item = {.a = 1, .b = 2};
        ringbuffer_item item_pop = {.a = 0, .b = 0};

        REQUIRE(uni_hal_ringbuffer_push(&ringbuffer_ctx, (uint8_t *)&item, 1U));
        REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 1);
        REQUIRE(uni_hal_ringbuffer_pop(&ringbuffer_ctx, (uint8_t *)&item_pop, 1U));
        REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 0);
        REQUIRE(memcmp(&item, &item_pop, sizeof(item_pop)) == 0);
    }

    SECTION("ok-2") {
        rb_init();

        ringbuffer_item item_1[3] = { {.a = 1, .b = 2}, {.a = 3, .b = 4}, {.a = 5, .b = 6} };
        ringbuffer_item item_2[3]{};

        REQUIRE_FALSE(memcmp(item_1, item_2, sizeof(item_1)) == 0);

        REQUIRE(uni_hal_ringbuffer_push(&ringbuffer_ctx, (uint8_t *)item_1, 3U));
        REQUIRE(uni_hal_ringbuffer_pop(&ringbuffer_ctx, (uint8_t *)item_2, 3U));
        REQUIRE(memcmp(item_1, item_2, sizeof(item_1)) == 0);
    }
}


TEST_CASE("ringbuffer_overflow", "[ringbuffer]") {
    // init
    rb_init();
    REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 0);
    REQUIRE(uni_hal_ringbuffer_is_empty(&ringbuffer_ctx));

    // fill 5 elements without overflow
    for (size_t i = 0; i < 5; i++) {
        // check current length
        REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == i);

        // push element
        ringbuffer_item item = {.a = i, .b = i + 1};
        REQUIRE(uni_hal_ringbuffer_push(&ringbuffer_ctx, (uint8_t *)&item, 1U) == true);

        // check length after push
        REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == i + 1);
    }

    // check that ringbuffer is full
    REQUIRE_FALSE(uni_hal_ringbuffer_is_empty(&ringbuffer_ctx));
    REQUIRE(uni_hal_ringbuffer_is_full(&ringbuffer_ctx) == true);

    // fill with overflow
    ringbuffer_item item = {.a = 5, .b = 6};
    REQUIRE(uni_hal_ringbuffer_push(&ringbuffer_ctx, (uint8_t *)&item, 1U) == true);

    // check length after push
    REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 5);

    // readback
    for (size_t i = 1; i < 6; i++) {
        REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 6 - i);
        ringbuffer_item item_r{};
        REQUIRE(uni_hal_ringbuffer_pop(&ringbuffer_ctx, (uint8_t *)&item_r, 1U) == true);
        REQUIRE((item_r.a == i && item_r.b == i + 1));
    }
    REQUIRE(uni_hal_ringbuffer_length(&ringbuffer_ctx) == 0);
}

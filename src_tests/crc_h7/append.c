#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#include <stm32h7xx_ll_crc.h>
#pragma GCC diagnostic pop

#include "../arm/semihost_test.h"

// The non-reflected CRC peripheral consumes the most significant byte of each write first.
static uint8_t written[16];
static unsigned count;
static void feed(uint32_t value, unsigned bytes) {
    while (bytes > 0U) {
        --bytes;
        if (count < sizeof(written)) {
            written[count] = (uint8_t)(value >> (8U * bytes));
        }
        ++count;
    }
}

#define LL_CRC_FeedData8(unit, value)  feed((value), 1U)
#define LL_CRC_FeedData16(unit, value) feed((value), 2U)
#define LL_CRC_FeedData32(unit, value) feed((value), 4U)
#include "crc/uni_hal_crc_stm32h7.c"

static unsigned check(void) {
    alignas(4) static const uint8_t data[] = {
        0x80, 0x91, 0xA2, 0xB3, 0xC4, 0xD5, 0xE6, 0xF7,
        0x88, 0x79, 0x6A, 0x5B, 0x4C, 0x3D, 0x2E, 0x1F,
    };
    uni_hal_crc_context_t ctx = {.initialized = true};
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;
    __DSB();
    __ISB();
    for (unsigned start = 0; start < 4U; ++start) {
        for (unsigned len = 0; len <= sizeof(data) - start; ++len) {
            count = 0;
            CHECK(uni_hal_crc_append(&ctx, &data[start], len));
            CHECK(count == len);
            for (unsigned i = 0; i < len; ++i) {
                CHECK(written[i] == data[start + i]);
            }
        }
    }
    return 0;
}

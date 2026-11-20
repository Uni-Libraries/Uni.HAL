#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#elif defined(__linux__)
#include <errno.h>
#include <sys/random.h>
#elif defined(__APPLE__)
#include <sys/random.h>
#else
#include <unistd.h>
#endif

static unsigned calls;
static unsigned fail_call;
static uint8_t next_byte;

static int fill_bytes(uint8_t *buf, size_t size) {
    if (++calls == fail_call) {
        return -1;
    }
    for (size_t i = 0; i < size; ++i) {
        buf[i] = next_byte++;
    }
    return 0;
}

#ifdef _WIN32
static NTSTATUS model_random(BCRYPT_ALG_HANDLE provider, PUCHAR buf, ULONG size, ULONG flags) {
    if (provider || flags != BCRYPT_USE_SYSTEM_PREFERRED_RNG) {
        return -1;
    }
    return fill_bytes(buf, size);
}
#define BCryptGenRandom model_random
#elif defined(__linux__)
static bool interrupt_next;
static ssize_t model_random(void *buf, size_t size, unsigned flags) {
    if (interrupt_next) {
        interrupt_next = false;
        errno = EINTR;
        return -1;
    }
    // Force short successful reads to exercise pointer and remaining-length updates.
    size_t count = size > 128U ? 128U : size;
    if (flags != GRND_NONBLOCK || fill_bytes(buf, count) < 0) {
        errno = EAGAIN;
        return -1;
    }
    return (ssize_t)count;
}
#define getrandom model_random
#else
static int model_random(void *buf, size_t size) {
    return size <= 256U ? fill_bytes(buf, size) : -1;
}
#define getentropy model_random
#endif

#include "rng/uni_hal_rng_pc.c"

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void) {
    uni_hal_rng_context_t ctx = {0};
    uint8_t buf[519] = {0};
    CHECK(uni_hal_rng_init(&ctx) && uni_hal_rng_is_inited(&ctx));
#if defined(__linux__)
    interrupt_next = true;
#endif
    next_byte = 37U;
    CHECK(uni_hal_rng_get(&ctx, buf + 1, sizeof(buf) - 2U));
    for (size_t i = 1; i + 1U < sizeof(buf); ++i) {
        CHECK(buf[i] == (uint8_t)(36U + i));
    }
    CHECK(!buf[0] && !buf[sizeof(buf) - 1U]);

    // The scalar API must preserve all bits. Use native byte order for the expected word.
    next_byte = 0xA1;
    CHECK(uni_hal_rng_get_8u(&ctx) == 0xA1);
    const uint8_t bytes[] = {0xA1, 0xA2, 0xA3, 0xA4};
    uint16_t word16;
    uint32_t word32;
    memcpy(&word16, bytes, sizeof(word16));
    memcpy(&word32, bytes, sizeof(word32));
    next_byte = 0xA1;
    CHECK(uni_hal_rng_get_16u(&ctx) == word16);
    next_byte = 0xA1;
    CHECK(uni_hal_rng_get_32u(&ctx) == word32);

#ifdef _WIN32
    fail_call = calls + 1U;
#else
    fail_call = calls + 2U; // Fail after one successful chunk.
#endif
    CHECK(!uni_hal_rng_get(&ctx, buf, sizeof(buf)));
    fail_call = calls + 1U;
    CHECK(uni_hal_rng_get_32u(&ctx) == 0U);
    fail_call = calls + 1U;
    CHECK(!uni_hal_rng_init(&ctx) && !uni_hal_rng_is_inited(&ctx));
    CHECK(!uni_hal_rng_get(&ctx, buf, 1) && !uni_hal_rng_get_16u(&ctx));
    CHECK(!uni_hal_rng_get_8u(NULL));
    return 0;
}

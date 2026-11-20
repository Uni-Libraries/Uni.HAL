#include "rng/uni_hal_rng.h"

#include <limits.h>

#if defined(_WIN32)
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

static bool read_entropy(uint8_t *buf, size_t size) {
    while (size > 0U) {
#if defined(_WIN32)
        const size_t chunk = size > ULONG_MAX ? ULONG_MAX : size;
        if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, buf, (ULONG)chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
            return false;
        }
#elif defined(__linux__)
        const size_t requested = size > INT_MAX ? INT_MAX : size;
        ssize_t const count = getrandom(buf, requested, GRND_NONBLOCK);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        const size_t chunk = (size_t)count;
#else
        // getentropy fills at most 256 bytes per call.
        const size_t chunk = size > 256U ? 256U : size;
        if (getentropy(buf, chunk) != 0) {
            return false;
        }
#endif
        buf += chunk;
        size -= chunk;
    }
    return true;
}

bool uni_hal_rng_init(uni_hal_rng_context_t *ctx) {
    if (!ctx) {
        return false;
    }
    uint8_t probe;
    ctx->inited = read_entropy(&probe, sizeof(probe));
    return ctx->inited;
}

bool uni_hal_rng_is_inited(const uni_hal_rng_context_t *ctx) {
    return ctx && ctx->inited;
}

bool uni_hal_rng_get(uni_hal_rng_context_t *ctx, uint8_t *buf, size_t buf_len) {
    return uni_hal_rng_is_inited(ctx) && buf && read_entropy(buf, buf_len);
}

uint8_t uni_hal_rng_get_8u(uni_hal_rng_context_t *ctx) {
    uint8_t value;
    return uni_hal_rng_get(ctx, &value, sizeof(value)) ? value : 0U;
}

uint16_t uni_hal_rng_get_16u(uni_hal_rng_context_t *ctx) {
    uint16_t value;
    return uni_hal_rng_get(ctx, (uint8_t *)&value, sizeof(value)) ? value : 0U;
}

uint32_t uni_hal_rng_get_32u(uni_hal_rng_context_t *ctx) {
    uint32_t value;
    return uni_hal_rng_get(ctx, (uint8_t *)&value, sizeof(value)) ? value : 0U;
}

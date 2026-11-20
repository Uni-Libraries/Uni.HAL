#include "rng/uni_hal_rng.h"

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void) {
    uni_hal_rng_context_t ctx = {0};
    uint8_t buf[519] = {0xAE};
    buf[sizeof(buf) - 1U] = 0x13;
    CHECK(!uni_hal_rng_init(NULL) && !uni_hal_rng_is_inited(&ctx));
    CHECK(!uni_hal_rng_get(&ctx, buf, sizeof(buf)));
    CHECK(uni_hal_rng_init(&ctx) && uni_hal_rng_is_inited(&ctx));
    CHECK(uni_hal_rng_get(&ctx, buf + 1, sizeof(buf) - 2U));
    CHECK(buf[0] == 0xAE && buf[sizeof(buf) - 1U] == 0x13);
    CHECK(uni_hal_rng_get(&ctx, buf, 0));
    CHECK(!uni_hal_rng_get(&ctx, NULL, 1));
    CHECK(!uni_hal_rng_get(&ctx, NULL, 0));
    return 0;
}

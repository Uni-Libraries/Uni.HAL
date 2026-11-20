#undef NDEBUG
#include <assert.h>

#include "dwt/uni_hal_dwt.h"
#include "i2c/uni_hal_i2c.h"

static uni_hal_gpio_pin_context_t scl, sda;
static bool scl_released, sda_released, target_holds_sda;
static uint32_t now_us, scl_high_at, stretch_us;
static unsigned clocks, clocks_to_free_sda, stops;

static bool clock_high(void) {
    return scl_released && now_us >= scl_high_at;
}

bool uni_hal_gpio_pin_init(uni_hal_gpio_pin_context_t *pin) {
    assert(pin->gpio_type == UNI_HAL_GPIO_TYPE_OUT_OD && pin->gpio_init);
    pin->inited = true;
    return true;
}

bool uni_hal_gpio_pin_deinit(uni_hal_gpio_pin_context_t *pin) {
    pin->inited = false;
    return true;
}

bool uni_hal_gpio_pin_get(const uni_hal_gpio_pin_context_t *pin) {
    assert(pin->inited);
    return pin == &scl ? clock_high() : (sda_released && !target_holds_sda);
}

bool uni_hal_gpio_pin_set(uni_hal_gpio_pin_context_t *pin, bool release) {
    assert(pin->inited);
    if (pin == &scl) {
        if (release && !scl_released) {
            scl_high_at = now_us + stretch_us;
            if (!stretch_us) {
                ++clocks;
            }
        }
        if (!release && clock_high() && clocks >= clocks_to_free_sda) {
            target_holds_sda = false;
        }
        scl_released = release;
    } else {
        // Only the physical SDA rising edge with SCL high terminates the transaction.
        if (release && !sda_released && !target_holds_sda && clock_high()) {
            assert(now_us - scl_high_at >= 4U); // Standard-mode STOP setup time
            ++stops;
        }
        sda_released = release;
    }
    return true;
}

bool uni_hal_dwt_init(void) { return true; }
bool uni_hal_dwt_is_inited(void) { return true; }

void uni_hal_dwt_delay_us(uint32_t us) {
    bool const was_high = clock_high();
    now_us += us;
    if (!was_high && clock_high()) {
        ++clocks;
    }
}

static bool recover(unsigned held_clocks, uint32_t hold_us) {
    scl.gpio_type = sda.gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_OD;
    scl.gpio_init = true;
    sda.gpio_init = false;
    clocks = stops = now_us = scl_high_at = 0;
    clocks_to_free_sda = held_clocks;
    target_holds_sda = held_clocks != 0;
    stretch_us = hold_us;
    scl_released = sda_released = true;
    uni_hal_i2c_context_t ctx = {.config = {.pin_sck = &scl, .pin_sda = &sda}};
    bool const recovered = uni_hal_i2c_recover(&ctx);
    assert(ctx.state.initialized);
    assert(scl.gpio_type == UNI_HAL_GPIO_TYPE_ALTERNATE_OD && scl.gpio_init);
    assert(sda.gpio_type == UNI_HAL_GPIO_TYPE_ALTERNATE_OD && !sda.gpio_init);
    return recovered;
}

int main(void) {
    assert(recover(0, 0) && stops == 1);
    // SCL rises after the old 50 us STOP setup delay, but before its final idle check.
    assert(recover(0, 75));
    assert(stops == 1);
    // The target only observes real clock edges, including on the ninth pulse.
    assert(recover(9, 75) && clocks == 10 && stops == 1);
    assert(!recover(0, 1000000) && stops == 0 && now_us < 3000);
    // Several short stretches can also exhaust the shared recovery budget.
    assert(!recover(9, 200) && clocks < 9 && stops == 0 && now_us < 2200);
    assert(!recover(20, 0) && clocks == 10 && stops == 0);
    return 0;
}

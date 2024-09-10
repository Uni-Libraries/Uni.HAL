//
// Includes
//

#include "gpio/uni_hal_gpio.h"


//
// Functions/Public
//

bool uni_hal_gpio_pin_init(uni_hal_gpio_pin_context_t* ctx_pin) {
    bool result = false;
    if (ctx_pin != nullptr) {
        ctx_pin->inited = true;
        result = true;
    }

    return result;
}


bool uni_hal_gpio_pin_is_inited(const uni_hal_gpio_pin_context_t* ctx_pin){
    bool result = false;
    if (ctx_pin != nullptr) {
        result = ctx_pin->inited;
    }

    return result;
}


bool uni_hal_gpio_pin_get(const uni_hal_gpio_pin_context_t* ctx_pin) {
    bool result = false;

    if (uni_hal_gpio_pin_is_inited(ctx_pin)) {
        result = true;
    }

    return result;
}


bool uni_hal_gpio_pin_set(uni_hal_gpio_pin_context_t* ctx_pin, bool val) {
    (void)val;

    bool result = false;

    if (uni_hal_gpio_pin_is_inited(ctx_pin)) {
        result = true;
    }

    return result;
}

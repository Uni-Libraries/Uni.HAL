#pragma once

#include "gpio/uni_hal_gpio.h"

static bool gpio_ok = true;
static unsigned gpio_calls;
static uni_hal_gpio_pin_context_t gpio_pin;

bool uni_hal_gpio_pin_init(uni_hal_gpio_pin_context_t *pin) {
    ++gpio_calls;
    gpio_pin = *pin;
    return gpio_ok;
}

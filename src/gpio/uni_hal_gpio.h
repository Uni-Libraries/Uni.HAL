#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stdint.h>

// uni_hal
#include "core/uni_hal_core.h"



//
// Typedefs
//

/**
 * GPIO pin type
 */
typedef enum{

    /**
     * Analog
     */
    UNI_HAL_GPIO_TYPE_ANALOG,

    /**
     * Input Digital
     */
    UNI_HAL_GPIO_TYPE_IN_DIGITAL,

    /**
     * Output Open Drain
     */
    UNI_HAL_GPIO_TYPE_OUT_OD,

    /**
     * Output Push-Pull
     */
    UNI_HAL_GPIO_TYPE_OUT_PP,

    /**
     * Alternate function Open Drain
     */
    UNI_HAL_GPIO_TYPE_ALTERNATE_OD,

    /**
     * Alternate function Push-Pull
     */
    UNI_HAL_GPIO_TYPE_ALTERNATE_PP,

} uni_hal_gpio_type_e;


/**
 * GPIO pin alternate function
 */
typedef enum {
    UNI_HAL_GPIO_ALTERNATE_0 = 0,
    UNI_HAL_GPIO_ALTERNATE_1 = 1,
    UNI_HAL_GPIO_ALTERNATE_2 = 2,
    UNI_HAL_GPIO_ALTERNATE_3 = 3,
    UNI_HAL_GPIO_ALTERNATE_4 = 4,
    UNI_HAL_GPIO_ALTERNATE_5 = 5,
    UNI_HAL_GPIO_ALTERNATE_6 = 6,
    UNI_HAL_GPIO_ALTERNATE_7 = 7,
    UNI_HAL_GPIO_ALTERNATE_8 = 8,
    UNI_HAL_GPIO_ALTERNATE_9 = 9,
    UNI_HAL_GPIO_ALTERNATE_10 = 10,
    UNI_HAL_GPIO_ALTERNATE_11 = 11,
    UNI_HAL_GPIO_ALTERNATE_12 = 12,
    UNI_HAL_GPIO_ALTERNATE_13 = 13,
    UNI_HAL_GPIO_ALTERNATE_14 = 14,
    UNI_HAL_GPIO_ALTERNATE_15 = 15,
} uni_hal_gpio_alternate_e;

/**
 * GPIO pin
 */
typedef enum {
    UNI_HAL_GPIO_PIN_0 = 1U << 0, //-V2572
    UNI_HAL_GPIO_PIN_1 = 1U << 1, //-V2572
    UNI_HAL_GPIO_PIN_2 = 1U << 2, //-V2572
    UNI_HAL_GPIO_PIN_3 = 1U << 3, //-V2572
    UNI_HAL_GPIO_PIN_4 = 1U << 4, //-V2572
    UNI_HAL_GPIO_PIN_5 = 1U << 5, //-V2572
    UNI_HAL_GPIO_PIN_6 = 1U << 6, //-V2572
    UNI_HAL_GPIO_PIN_7 = 1U << 7, //-V2572
    UNI_HAL_GPIO_PIN_8 = 1U << 8, //-V2572
    UNI_HAL_GPIO_PIN_9 = 1U << 9, //-V2572
    UNI_HAL_GPIO_PIN_10 = 1U << 10, //-V2572
    UNI_HAL_GPIO_PIN_11 = 1U << 11, //-V2572
    UNI_HAL_GPIO_PIN_12 = 1U << 12, //-V2572
    UNI_HAL_GPIO_PIN_13 = 1U << 13, //-V2572
    UNI_HAL_GPIO_PIN_14 = 1U << 14, //-V2572
    UNI_HAL_GPIO_PIN_15 = 1U << 15, //-V2572
} uni_hal_gpio_pin_e;

typedef enum {
    UNI_HAL_GPIO_SPEED_0,
    UNI_HAL_GPIO_SPEED_1,
    UNI_HAL_GPIO_SPEED_2,
    UNI_HAL_GPIO_SPEED_3,
} uni_hal_gpio_speed_e;

/**
 * GPIO Pull Up/Down configuration
 */
typedef enum{
    /**
     * Disable pull
     */
    UNI_HAL_GPIO_PULL_NO,

    /**
     * Pull down
     */
    UNI_HAL_GPIO_PULL_DOWN,

    /**
     * Pull up
     */
    UNI_HAL_GPIO_PULL_UP,
} uni_hal_gpio_pull_e;


/**
 * GPIO pin structure
 */
typedef struct {
    /**
     * GPIO bank
     */
    uni_hal_core_periph_e gpio_bank;

    /**
     * Number of pin or pins in bank
     */
    uni_hal_gpio_pin_e gpio_pin;

    /**
     * Pin type
     */
    uni_hal_gpio_type_e gpio_type;

    /**
     * GPIO alternate function
     */
    uni_hal_gpio_alternate_e alternate;

    /**
     * Pin pull direction
     */
    uni_hal_gpio_pull_e gpio_pull;

    /**
     * GPIO driver strength
     */
    uni_hal_gpio_speed_e gpio_speed;

    /**
     * Pin initial state
     */
    bool gpio_init;

    /**
     * Initialization state
     */
    bool inited;
} uni_hal_gpio_pin_context_t;


//
// Definitions
//

#define UNI_HAL_GPIO_PIN_DEFINITION(name, bank, pin)   \
uni_hal_gpio_pin_context_t name##_ctx = {              \
    .gpio_bank = bank,                                \
    .gpio_pin = pin                                   \
}

#define UNI_HAL_GPIO_PIN_DECLARATION(name)             \
extern uni_hal_gpio_pin_context_t name##_ctx



//
// Functions
//

bool uni_hal_gpio_pin_init(uni_hal_gpio_pin_context_t* ctx_pin);

/**
 * Checks that GPIO pin was properly initialized
 * @param ctx_pin GPIO pin context
 * @return true on success
 */
bool uni_hal_gpio_pin_is_inited(const uni_hal_gpio_pin_context_t* ctx_pin);

/**
 * Gets current pin status
 * @param ctx_pin pointer to GPIO pint context structure
 * @return true for HIGH, false for LOW
 */
bool uni_hal_gpio_pin_get(const uni_hal_gpio_pin_context_t* ctx_pin);

/**
 * Enables or disables GPIO pin
 * @param ctx_pin pointer to GPIO pint context structure
 * @param val value, true to HIGH, false to LOW
 * @return true on success
 */
bool uni_hal_gpio_pin_set(uni_hal_gpio_pin_context_t* ctx_pin, bool val);

#if defined(__cplusplus)
}
#endif

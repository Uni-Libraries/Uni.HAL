//
// Includes
//

// stdlib
#include <stddef.h>

// ST
#include <stm32h7xx.h>
#include <stm32h7xx_ll_gpio.h>

// uni_hal
#include "core/uni_hal_core.h"
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"



//
// Private
//

/**
 * Get GPIO bank handle from the peripheral enum
 * @param bank peripheral enum val
 * @return pointer to the GPIO bank handle in case of success
 */
static GPIO_TypeDef *_uni_hal_gpio_bank(uni_hal_core_periph_e bank) {
    GPIO_TypeDef *result;

    switch (bank) {
        case UNI_HAL_CORE_PERIPH_GPIO_A:
            result = GPIOA; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_B:
            result = GPIOB; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_C:
            result = GPIOC; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_D:
            result = GPIOD; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_E:
            result = GPIOE; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_F:
            result = GPIOF; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_G:
            result = GPIOG; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_H:
            result = GPIOH; //-V2571
            break;
        case UNI_HAL_CORE_PERIPH_GPIO_I:
            result = GPIOI; //-V2571
            break;
        default:
            result = NULL;
            break;
    }

    return result;
}

static uint32_t _uni_hal_gpio_type(uni_hal_gpio_type_e type) {
    uint32_t result;

    switch (type) {
    case UNI_HAL_GPIO_TYPE_OUT_OD:
    case UNI_HAL_GPIO_TYPE_OUT_PP:
        result = LL_GPIO_MODE_OUTPUT;
        break;
    case UNI_HAL_GPIO_TYPE_IN_DIGITAL:
        result = LL_GPIO_MODE_INPUT;
        break;
    case UNI_HAL_GPIO_TYPE_ALTERNATE_OD:
    case UNI_HAL_GPIO_TYPE_ALTERNATE_PP:
        result = LL_GPIO_MODE_ALTERNATE;
        break;
    case UNI_HAL_GPIO_TYPE_ANALOG:
    default:
        result = LL_GPIO_MODE_ANALOG;
        break;
    }

    return result;
}

static uint32_t _uni_hal_gpio_mode(uni_hal_gpio_type_e type) {
    uint32_t result;

    switch (type) {
        case UNI_HAL_GPIO_TYPE_OUT_OD:
        case UNI_HAL_GPIO_TYPE_ALTERNATE_OD:
            result = LL_GPIO_OUTPUT_OPENDRAIN;
            break;
        case UNI_HAL_GPIO_TYPE_ANALOG:
        case UNI_HAL_GPIO_TYPE_IN_DIGITAL:
        case UNI_HAL_GPIO_TYPE_OUT_PP:
        case UNI_HAL_GPIO_TYPE_ALTERNATE_PP:
        default:
            result = LL_GPIO_OUTPUT_PUSHPULL;
            break;
    }

    return result;
}

static uint32_t _uni_hal_gpio_pull(uni_hal_gpio_pull_e val) {
    uint32_t result;

    switch (val) {
        case UNI_HAL_GPIO_PULL_DOWN:
            result = LL_GPIO_PULL_DOWN;
            break;
        case UNI_HAL_GPIO_PULL_UP:
            result = LL_GPIO_PULL_UP;
            break;
        case UNI_HAL_GPIO_PULL_NO:
        default:
            result = LL_GPIO_PULL_NO;
            break;
    }

    return result;
}

static uint32_t _uni_hal_gpio_alternate(uni_hal_gpio_alternate_e alternate){
    uint32_t result = LL_GPIO_AF_0;
    switch(alternate){
    case UNI_HAL_GPIO_ALTERNATE_0:
        result = LL_GPIO_AF_0;
        break;
    case UNI_HAL_GPIO_ALTERNATE_1:
        result = LL_GPIO_AF_1;
        break;
    case UNI_HAL_GPIO_ALTERNATE_2:
        result = LL_GPIO_AF_2;
        break;
    case UNI_HAL_GPIO_ALTERNATE_3:
        result = LL_GPIO_AF_3;
        break;
    case UNI_HAL_GPIO_ALTERNATE_4:
        result = LL_GPIO_AF_4;
        break;
    case UNI_HAL_GPIO_ALTERNATE_5:
        result = LL_GPIO_AF_5;
        break;
    case UNI_HAL_GPIO_ALTERNATE_6:
        result = LL_GPIO_AF_6;
        break;
    case UNI_HAL_GPIO_ALTERNATE_7:
        result = LL_GPIO_AF_7;
        break;
    case UNI_HAL_GPIO_ALTERNATE_8:
        result = LL_GPIO_AF_8;
        break;
    case UNI_HAL_GPIO_ALTERNATE_9:
        result = LL_GPIO_AF_9;
        break;
    case UNI_HAL_GPIO_ALTERNATE_10:
        result = LL_GPIO_AF_10;
        break;
    case UNI_HAL_GPIO_ALTERNATE_11:
        result = LL_GPIO_AF_11;
        break;
    case UNI_HAL_GPIO_ALTERNATE_12:
        result = LL_GPIO_AF_12;
        break;
    case UNI_HAL_GPIO_ALTERNATE_13:
        result = LL_GPIO_AF_13;
        break;
    case UNI_HAL_GPIO_ALTERNATE_14:
        result = LL_GPIO_AF_14;
        break;
    case UNI_HAL_GPIO_ALTERNATE_15:
        result = LL_GPIO_AF_15;
        break;
    default:
        // unknown alternate function
        break;
    }

    return result;
}


static uint32_t _uni_hal_gpio_speed(uni_hal_gpio_speed_e speed){
    uint32_t result = LL_GPIO_SPEED_FREQ_LOW;
    switch(speed){
        case UNI_HAL_GPIO_SPEED_0:
            result =LL_GPIO_SPEED_FREQ_LOW;
            break;
        case UNI_HAL_GPIO_SPEED_1:
            result =LL_GPIO_SPEED_FREQ_MEDIUM;
            break;
        case UNI_HAL_GPIO_SPEED_2:
            result =LL_GPIO_SPEED_FREQ_HIGH;
            break;
        case UNI_HAL_GPIO_SPEED_3:
            result =LL_GPIO_SPEED_FREQ_VERY_HIGH;
            break;
        default:
            break;
    }

    return result;
}



//
// Functions/Public
//

bool uni_hal_gpio_pin_init(uni_hal_gpio_pin_context_t *ctx) {
    bool result = false;
    if (ctx != NULL && !uni_hal_gpio_pin_is_inited(ctx)) {
        if(ctx->gpio_bank != UNI_HAL_CORE_PERIPH_EMPTY) {
            GPIO_TypeDef *bank = _uni_hal_gpio_bank(ctx->gpio_bank);
            if (bank != NULL) {
                result = uni_hal_rcc_clk_set(ctx->gpio_bank, true);

                ctx->gpio_init ? LL_GPIO_SetOutputPin(bank, (uint32_t)ctx->gpio_pin)
                               : LL_GPIO_ResetOutputPin(bank, (uint32_t)ctx->gpio_pin);
                LL_GPIO_InitTypeDef GPIO_InitStruct = {
                    .Pin = ctx->gpio_pin,
                    .Mode = _uni_hal_gpio_type(ctx->gpio_type),
                    .Speed = _uni_hal_gpio_speed(ctx->gpio_speed),
                    .OutputType = _uni_hal_gpio_mode(ctx->gpio_type),
                    .Pull = _uni_hal_gpio_pull(ctx->gpio_pull),
                    .Alternate = _uni_hal_gpio_alternate(ctx->alternate),
                };

                result = (LL_GPIO_Init(bank, &GPIO_InitStruct) == SUCCESS) && result;
            }
        }
        else{
            result = true;
        }
        ctx->inited = result;
    }

    return result;
}



bool uni_hal_gpio_pin_is_inited(const uni_hal_gpio_pin_context_t *ctx) {
    bool result =false;
    if(ctx != NULL){
        result = ctx->inited;
    }
    return result;
}

bool uni_hal_gpio_pin_get(const uni_hal_gpio_pin_context_t *ctx_pin) {
    bool result = false;

    if (ctx_pin != NULL) {
        if(ctx_pin->gpio_bank != UNI_HAL_CORE_PERIPH_EMPTY) {
            result = (LL_GPIO_ReadInputPort(_uni_hal_gpio_bank(ctx_pin->gpio_bank)) & ((uint32_t)ctx_pin->gpio_pin)) != 0U;
        }
        else{
            result = true;
        }
    }

    return result;
}

bool uni_hal_gpio_pin_set(uni_hal_gpio_pin_context_t *ctx_pin, bool val) {
    bool result = false;

    if (ctx_pin != NULL) {
        if (ctx_pin->gpio_bank != UNI_HAL_CORE_PERIPH_EMPTY) {
            if (val) {
                LL_GPIO_SetOutputPin(_uni_hal_gpio_bank(ctx_pin->gpio_bank), (uint32_t)ctx_pin->gpio_pin);
            } else {
                LL_GPIO_ResetOutputPin(_uni_hal_gpio_bank(ctx_pin->gpio_bank), (uint32_t)ctx_pin->gpio_pin);
            }
        }
        result = true;
    }

    return result;
}

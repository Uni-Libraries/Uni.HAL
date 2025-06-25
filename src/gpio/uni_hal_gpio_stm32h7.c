//
// Includes
//

// stdlib
#include <stddef.h>
#include <stdbool.h>
 
// ST
#include <stm32h7xx.h>
#include <stm32h7xx_ll_gpio.h>
#include <stm32h7xx_ll_exti.h>
#include <stm32h7xx_ll_system.h>
 
// FreeRTOS
#include <FreeRTOS.h>

// uni_hal
#include "core/uni_hal_core.h"
#include "gpio/uni_hal_gpio.h"
#include "rcc/uni_hal_rcc.h"



//
// Defines
//

#define UNI_HAL_GPIO_IT_PRIORITY 5U



//
// Globals
//

static uni_hal_gpio_pin_context_t *g_uni_hal_gpio_ctx[16] = {NULL};



//
// IRQ
//





//
// Interrupts

//
static inline bool EXTIX_IRQHandler(uint32_t line, size_t index) {
    bool result = false;
    if (LL_EXTI_IsActiveFlag_0_31(line)) {
        LL_EXTI_ClearFlag_0_31(line);
        if (g_uni_hal_gpio_ctx[index] && g_uni_hal_gpio_ctx[index]->callback) {
            result = g_uni_hal_gpio_ctx[index]->callback(g_uni_hal_gpio_ctx[index]->callback_cookie);
        }
    }
    return result;
}

void EXTI0_IRQHandler(void) {
    traceISR_ENTER();
    portYIELD_FROM_ISR(EXTIX_IRQHandler(LL_EXTI_LINE_0, 0));
}

void EXTI1_IRQHandler(void) {
    traceISR_ENTER();
    portYIELD_FROM_ISR(EXTIX_IRQHandler(LL_EXTI_LINE_1, 1));
}

void EXTI2_IRQHandler(void) {
    traceISR_ENTER();
    portYIELD_FROM_ISR(EXTIX_IRQHandler(LL_EXTI_LINE_2, 2));
}

void EXTI3_IRQHandler(void) {
    traceISR_ENTER();
    portYIELD_FROM_ISR(EXTIX_IRQHandler(LL_EXTI_LINE_3, 3));
}

void EXTI4_IRQHandler(void) {
    traceISR_ENTER();
    portYIELD_FROM_ISR(EXTIX_IRQHandler(LL_EXTI_LINE_4, 4));
}

void EXTI9_5_IRQHandler(void) {
    traceISR_ENTER();
    bool wakeup = false;
    wakeup = EXTIX_IRQHandler(LL_EXTI_LINE_5, 5) || wakeup;
    wakeup = EXTIX_IRQHandler(LL_EXTI_LINE_6, 6) || wakeup;
    wakeup = EXTIX_IRQHandler(LL_EXTI_LINE_7, 7) || wakeup;
    wakeup = EXTIX_IRQHandler(LL_EXTI_LINE_8, 8) || wakeup;
    wakeup = EXTIX_IRQHandler(LL_EXTI_LINE_9, 9) || wakeup;
    portYIELD_FROM_ISR(wakeup);
}

void EXTI15_10_IRQHandler(void) {
    traceISR_ENTER();
    bool wakeup = false;
    wakeup = EXTIX_IRQHandler(LL_EXTI_LINE_10, 10) || wakeup;
    wakeup = EXTIX_IRQHandler(LL_EXTI_LINE_11, 11) || wakeup;
    wakeup = EXTIX_IRQHandler(LL_EXTI_LINE_12, 12) || wakeup;
    wakeup = EXTIX_IRQHandler(LL_EXTI_LINE_13, 13) || wakeup;
    wakeup = EXTIX_IRQHandler(LL_EXTI_LINE_14, 14) || wakeup;
    wakeup = EXTIX_IRQHandler(LL_EXTI_LINE_15, 15) || wakeup;
    portYIELD_FROM_ISR(wakeup);
}



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

static uint32_t _uni_hal_gpio_index(uni_hal_gpio_pin_e pin) {
    uint32_t result = UINT32_MAX;
    for (size_t i = 0; i < 16U; i++) {
        if (pin & (1 << i)) {
            result = i;
            break;
        }
    }
    return result;
}

static uint32_t _uni_hal_gpio_exti_port(uni_hal_core_periph_e bank) {
    switch (bank) {
        case UNI_HAL_CORE_PERIPH_GPIO_A: return LL_SYSCFG_EXTI_PORTA;
        case UNI_HAL_CORE_PERIPH_GPIO_B: return LL_SYSCFG_EXTI_PORTB;
        case UNI_HAL_CORE_PERIPH_GPIO_C: return LL_SYSCFG_EXTI_PORTC;
        case UNI_HAL_CORE_PERIPH_GPIO_D: return LL_SYSCFG_EXTI_PORTD;
        case UNI_HAL_CORE_PERIPH_GPIO_E: return LL_SYSCFG_EXTI_PORTE;
        case UNI_HAL_CORE_PERIPH_GPIO_F: return LL_SYSCFG_EXTI_PORTF;
        case UNI_HAL_CORE_PERIPH_GPIO_G: return LL_SYSCFG_EXTI_PORTG;
        case UNI_HAL_CORE_PERIPH_GPIO_H: return LL_SYSCFG_EXTI_PORTH;
        case UNI_HAL_CORE_PERIPH_GPIO_I: return LL_SYSCFG_EXTI_PORTI;
        default: return UINT32_MAX;
    }
}

static uint32_t _uni_hal_gpio_exti_line(uni_hal_gpio_pin_e pin) {
    switch (pin) {
        case UNI_HAL_GPIO_PIN_0: return LL_EXTI_LINE_0;
        case UNI_HAL_GPIO_PIN_1: return LL_EXTI_LINE_1;
        case UNI_HAL_GPIO_PIN_2: return LL_EXTI_LINE_2;
        case UNI_HAL_GPIO_PIN_3: return LL_EXTI_LINE_3;
        case UNI_HAL_GPIO_PIN_4: return LL_EXTI_LINE_4;
        case UNI_HAL_GPIO_PIN_5: return LL_EXTI_LINE_5;
        case UNI_HAL_GPIO_PIN_6: return LL_EXTI_LINE_6;
        case UNI_HAL_GPIO_PIN_7: return LL_EXTI_LINE_7;
        case UNI_HAL_GPIO_PIN_8: return LL_EXTI_LINE_8;
        case UNI_HAL_GPIO_PIN_9: return LL_EXTI_LINE_9;
        case UNI_HAL_GPIO_PIN_10: return LL_EXTI_LINE_10;
        case UNI_HAL_GPIO_PIN_11: return LL_EXTI_LINE_11;
        case UNI_HAL_GPIO_PIN_12: return LL_EXTI_LINE_12;
        case UNI_HAL_GPIO_PIN_13: return LL_EXTI_LINE_13;
        case UNI_HAL_GPIO_PIN_14: return LL_EXTI_LINE_14;
        case UNI_HAL_GPIO_PIN_15: return LL_EXTI_LINE_15;
        default: return UINT32_MAX;
    }
}

static uni_hal_core_irq_e _uni_hal_gpio_irqn(uni_hal_gpio_pin_e pin) {
    uni_hal_core_irq_e result = UNI_HAL_CORE_IRQ_UNKNOWN;
    switch (pin) {
        case UNI_HAL_GPIO_PIN_0: 
            result = UNI_HAL_CORE_IRQ_EXTI_0;
            break;
        case UNI_HAL_GPIO_PIN_1: 
            result = UNI_HAL_CORE_IRQ_EXTI_1;
            break;
        case UNI_HAL_GPIO_PIN_2: 
            result = UNI_HAL_CORE_IRQ_EXTI_2;
            break;
        case UNI_HAL_GPIO_PIN_3: 
            result = UNI_HAL_CORE_IRQ_EXTI_3;
            break;
        case UNI_HAL_GPIO_PIN_4: 
            result = UNI_HAL_CORE_IRQ_EXTI_4;
            break;
        case UNI_HAL_GPIO_PIN_5:
        case UNI_HAL_GPIO_PIN_6:
        case UNI_HAL_GPIO_PIN_7:
        case UNI_HAL_GPIO_PIN_8:
        case UNI_HAL_GPIO_PIN_9: 
            result = UNI_HAL_CORE_IRQ_EXTI_5;
            break;
        case UNI_HAL_GPIO_PIN_10:
        case UNI_HAL_GPIO_PIN_11:
        case UNI_HAL_GPIO_PIN_12:
        case UNI_HAL_GPIO_PIN_13:
        case UNI_HAL_GPIO_PIN_14:
        case UNI_HAL_GPIO_PIN_15: 
            result = UNI_HAL_CORE_IRQ_EXTI_10;
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
    bool result = false;
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

bool uni_hal_gpio_pin_set_interrupt_callback(uni_hal_gpio_pin_context_t* ctx, uni_hal_gpio_it_edge_e edge, uni_hal_gpio_callback_t cb, void* cookie) {
    bool result = false;
    if (uni_hal_gpio_pin_is_inited(ctx)) {
        uint32_t exti_port = _uni_hal_gpio_exti_port(ctx->gpio_bank);
        uint32_t exti_line = _uni_hal_gpio_exti_line(ctx->gpio_pin);
        uint32_t exti_index = _uni_hal_gpio_index(ctx->gpio_pin);
        uni_hal_core_irq_e exti_irqn = _uni_hal_gpio_irqn(ctx->gpio_pin);
        
        if (exti_line != UINT32_MAX && exti_port != UINT32_MAX && exti_irqn != UNI_HAL_CORE_IRQ_UNKNOWN && exti_index != UINT32_MAX) {
            ctx->callback = cb;
            ctx->callback_cookie = cookie;
            g_uni_hal_gpio_ctx[exti_index] = ctx;

            LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
            EXTI_InitStruct.Line_0_31 = exti_line;
            EXTI_InitStruct.LineCommand = ENABLE;
            EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
            switch(edge) {
                case UNI_HAL_GPIO_IT_EDGE_RISING:
                    EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING;
                    break;
                case UNI_HAL_GPIO_IT_EDGE_FALLING:
                    EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_FALLING;
                    break;
                case UNI_HAL_GPIO_IT_EDGE_BOTH:
                    EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING_FALLING;
                    break;
            }
            LL_EXTI_Init(&EXTI_InitStruct);
            LL_SYSCFG_SetEXTISource(exti_port, exti_line);
            uni_hal_core_irq_enable(exti_irqn, UNI_HAL_GPIO_IT_PRIORITY, 0);
            result = true;
        }
    }
    return result;
}

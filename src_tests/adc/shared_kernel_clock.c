#include "rcc_model.h"
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_adc.h>
#define FIRST_SOURCE UNI_HAL_RCC_CLKSRC_PLL3R
#define OTHER_SOURCE UNI_HAL_RCC_CLKSRC_CLKP
#define FIRST_SELECTOR LL_RCC_ADC_CLKSOURCE_PLL3R
#else
#include <stm32l4xx_ll_adc.h>
#define FIRST_SOURCE UNI_HAL_RCC_CLKSRC_SYSCLK
#define OTHER_SOURCE UNI_HAL_RCC_CLKSRC_NONE
#define FIRST_SELECTOR LL_RCC_ADC_CLKSOURCE_SYSCLK
#endif
#include "model.h"
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

static uni_hal_adc_context_t *during_setup;
static bool nested_started;

static void start_nested(void) {
    uni_hal_adc_context_t *other = during_setup;
    during_setup = nullptr;
    nested_started = uni_hal_adc_init(other);
}

static void schedule_nested(uni_hal_adc_context_t *other) {
    during_setup = other;
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    adc_delay_hook = start_nested;
#endif
}

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
ErrorStatus __real_LL_ADC_Init(ADC_TypeDef *adc, LL_ADC_InitTypeDef *config);
ErrorStatus __wrap_LL_ADC_Init(ADC_TypeDef *adc, LL_ADC_InitTypeDef *config) {
    if (during_setup) {
        start_nested();
    }
    return __real_LL_ADC_Init(adc, config);
}
#endif

static unsigned check_pair(uni_hal_core_periph_e first, uni_hal_core_periph_e second) {
    reset_adc_model();
    g_uni_hal_rcc_status.inited = true;
    uni_hal_adc_context_t owner = {.config = {
        .instance = first, .clock_source = FIRST_SOURCE, .channels = {1U}, .channels_count = 1U
    }};
    uni_hal_adc_context_t other = {.config = {
        .instance = second, .clock_source = OTHER_SOURCE, .channels = {2U}, .channels_count = 1U
    }};
    CHECK(uni_hal_adc_init(&owner));
    CHECK(LL_RCC_GetADCClockSource(LL_RCC_ADC_CLKSOURCE) == FIRST_SELECTOR);
    const ADC_TypeDef *adc = adc_model_registers(first);
    uint32_t const sequence = adc->SQR1;
    bool const started = uni_hal_adc_init(&other);
    CHECK(LL_RCC_GetADCClockSource(LL_RCC_ADC_CLKSOURCE) == FIRST_SELECTOR);
    CHECK(!started && !uni_hal_adc_is_inited(&other));
    CHECK((adc->CR & ADC_CR_ADSTART) && adc->SQR1 == sequence);
    other.config.clock_source = FIRST_SOURCE;
    CHECK(uni_hal_adc_init(&other));
    CHECK(adc_model_registers(second)->CR & ADC_CR_ADSTART);

    reset_adc_model();
    owner.state.initialized = false;
    other.state.initialized = false;
    other.config.clock_source = OTHER_SOURCE;
    schedule_nested(&other);
    nested_started = true;
    CHECK(uni_hal_adc_init(&owner));
    CHECK(!during_setup && !nested_started);
    CHECK(LL_RCC_GetADCClockSource(LL_RCC_ADC_CLKSOURCE) == FIRST_SELECTOR);
    return 0U;
}

static unsigned check_nested_divider(uni_hal_core_periph_e second, bool separate_block) {
    reset_adc_model();
    uni_hal_adc_context_t owner = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_1, .clock_source = FIRST_SOURCE,
        .clock_divider = 8U, .channels = {1U}, .channels_count = 1U
    }};
    uni_hal_adc_context_t other = {.config = {
        .instance = second, .clock_source = FIRST_SOURCE, .clock_divider = 16U,
        .channels = {2U}, .channels_count = 1U
    }};
    schedule_nested(&other);
    nested_started = !separate_block;
    CHECK(uni_hal_adc_init(&owner));
    CHECK(!during_setup && nested_started == separate_block);
    CHECK(LL_ADC_GetCommonClock(adc_model_common_registers()) == LL_ADC_CLOCK_ASYNC_DIV8);
    CHECK(uni_hal_adc_init(&other));
    CHECK(LL_ADC_GetCommonClock(adc_model_common_registers()) == LL_ADC_CLOCK_ASYNC_DIV8);
    return 0U;
}

unsigned test_adc(void) {
    unsigned line = check_pair(UNI_HAL_CORE_PERIPH_ADC_1, UNI_HAL_CORE_PERIPH_ADC_2);
    if (line) {
        return line;
    }
    // On H743 ADC3 has a separate bus gate and common block, but shares ADCSEL with ADC1/2.
    line = check_pair(UNI_HAL_CORE_PERIPH_ADC_3, UNI_HAL_CORE_PERIPH_ADC_1);
    if (line) {
        return line;
    }

    line = check_nested_divider(UNI_HAL_CORE_PERIPH_ADC_2, false);
    if (line) {
        return line;
    }
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    line = check_nested_divider(UNI_HAL_CORE_PERIPH_ADC_3, true);
    if (line) {
        return line;
    }
#endif

    // A synchronous conversion does not consume the kernel selector.
    reset_adc_model();
    uni_hal_adc_context_t sync = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_3, .clock_mode = UNI_HAL_ADC_CLOCK_SYNC,
        .clock_source = OTHER_SOURCE, .channels = {1U}, .channels_count = 1U
    }};
    uni_hal_adc_context_t regular = {.config = {
        .instance = UNI_HAL_CORE_PERIPH_ADC_1, .clock_source = FIRST_SOURCE, .channels = {2U}, .channels_count = 1U
    }};
    CHECK(uni_hal_adc_init(&sync) && uni_hal_adc_init(&regular));
    CHECK(LL_RCC_GetADCClockSource(LL_RCC_ADC_CLKSOURCE) == FIRST_SELECTOR);
    CHECK(adc_model_registers(sync.config.instance)->CR & ADC_CR_ADSTART);
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    // ADC3 can also select its own synchronous common clock while ADC12 uses the kernel clock.
    reset_adc_model();
    sync.state.initialized = regular.state.initialized = false;
    CHECK(uni_hal_adc_init(&regular) && uni_hal_adc_init(&sync));
    CHECK(LL_RCC_GetADCClockSource(LL_RCC_ADC_CLKSOURCE) == FIRST_SELECTOR);
#endif
    // A calibration timeout releases the software reservation for a different ADC and source.
    reset_adc_model();
    regular.state.initialized = sync.state.initialized = false;
    regular.config.timeout = 1U;
    adc_calibration_delay = 10U;
    CHECK(!uni_hal_adc_init(&regular));
    adc_calibration_delay = 0U;
    CHECK(uni_hal_adc_init(&sync));
    CHECK(LL_RCC_GetADCClockSource(LL_RCC_ADC_CLKSOURCE) != FIRST_SELECTOR);
    return 0U;
}

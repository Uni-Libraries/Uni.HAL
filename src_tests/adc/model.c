// Run the real ADC driver with ST's device and LL headers on a Cortex-M CPU.
// Both DMA blocks and the ADC registers live in RAM. The clock model completes ADC startup.
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx.h>
#else
#include <stm32l4xx.h>
#endif
#include <string.h>

static ADC_TypeDef adcs[3];
static ADC_Common_TypeDef common;
#undef ADC1
#undef ADC2
#undef ADC3
#define ADC1 (&adcs[0])
#define ADC2 (&adcs[1])
#define ADC3 (&adcs[2])

// One block covers every channel/stream and the L496 request-selection register.
typedef union {
    DMA_TypeDef control;
    uint32_t words[64];
} dma_bank_t;
static dma_bank_t dma_banks[2];
#undef DMA1
#undef DMA2
#define DMA1 (&dma_banks[0].control)
#define DMA2 (&dma_banks[1].control)
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
static DMAMUX_Channel_TypeDef mux[16];
#undef DMAMUX1_Channel0
#define DMAMUX1_Channel0 (&mux[0])
#endif

unsigned adc_peripheral_calls;
#undef NVIC_SetPriority
#undef NVIC_EnableIRQ
#define NVIC_SetPriority(irq, priority) ((void)(irq), (void)(priority), ++adc_peripheral_calls)
#define NVIC_EnableIRQ(irq) ((void)(irq), ++adc_peripheral_calls)

// Substitute the board services only within this driver's translation unit.
#define uni_hal_gpio_pin_init model_gpio_init
#define uni_hal_gpio_pin_is_inited model_gpio_is_inited
#define uni_hal_rcc_clk_set model_clock
#define uni_hal_rcc_clksrc_set model_clock_source
#define uni_hal_systick_get_ms model_tick
#define uni_hal_dwt_delay_ms model_delay
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
static DBGMCU_TypeDef debug;
#undef DBGMCU
#define DBGMCU (&debug)
static ADC_Common_TypeDef common3;
#undef ADC12_COMMON
#undef ADC3_COMMON
#define ADC12_COMMON (&common)
#define ADC3_COMMON (&common3)
#include "dma/uni_hal_dma_stm32h7.c"
#else
#undef ADC123_COMMON
#define ADC123_COMMON (&common)
#include "dma/uni_hal_dma_stm32l4.c"
#endif

static bool model_dma_init(uni_hal_dma_context_t *dma);
#define uni_hal_dma_init model_dma_init
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "adc/uni_hal_adc_stm32h7.c"
#else
#include "adc/uni_hal_adc_stm32l4.c"
#endif
#undef uni_hal_dma_init

#include "model.h"

bool adc_dma_init_succeeds;

unsigned dma_init_calls;
static unsigned tick;

void reset_adc_model(void) {
    memset(adcs, 0, sizeof adcs);
    memset(dma_banks, 0, sizeof dma_banks);
    adc_dma_init_succeeds = false;
    memset(&common, 0, sizeof common);
    adc_peripheral_calls = 0;
    dma_init_calls = 0;
    tick = 0;
}

bool adc_model_untouched(void) {
    const ADC_TypeDef reset_adc[3] = {};
    const ADC_Common_TypeDef reset_common = {};
    const dma_bank_t reset_dma[2] = {};
    return adc_peripheral_calls == 0U && memcmp(adcs, reset_adc, sizeof adcs) == 0 &&
           memcmp(&common, &reset_common, sizeof common) == 0 &&
           memcmp(dma_banks, reset_dma, sizeof dma_banks) == 0;
}

const ADC_TypeDef *adc_model_registers(void) {
    return ADC1;
}

bool adc_model_capturing(const volatile uint16_t *data, uint32_t count) {
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    const uint32_t index = LL_DMA_STREAM_1;
    const bool enabled = LL_DMA_IsEnabledStream(DMA1, index);
#else
    const uint32_t index = LL_DMA_CHANNEL_1;
    const bool enabled = LL_DMA_IsEnabledChannel(DMA1, index);
#endif
    return (ADC1->CR & ADC_CR_ADSTART) && enabled &&
           LL_DMA_GetMemoryAddress(DMA1, index) == (uintptr_t)data && LL_DMA_GetDataLength(DMA1, index) == count;
}

bool model_gpio_init(uni_hal_gpio_pin_context_t *pin) {
    (void)pin;
    ++adc_peripheral_calls;
    return true;
}

bool model_gpio_is_inited(const uni_hal_gpio_pin_context_t *pin) {
    (void)pin;
    return false;
}

bool model_clock(uni_hal_core_periph_e instance, bool enable) {
    (void)instance;
    (void)enable;
    ++adc_peripheral_calls;
    return true;
}

bool model_clock_source(uni_hal_core_periph_e instance, uni_hal_rcc_clksrc_e source) {
    (void)instance;
    (void)source;
    ++adc_peripheral_calls;
    return true;
}

static bool model_dma_init(uni_hal_dma_context_t *dma) {
    ++dma_init_calls;
    return adc_dma_init_succeeds && uni_hal_dma_init(dma);
}

void model_delay(uint32_t ms) {
    (void)ms;
}

uint32_t model_tick(void) {
    ADC1->CR &= ~ADC_CR_ADCAL;
    if (ADC1->CR & ADC_CR_ADEN) {
        ADC1->ISR |= ADC_ISR_ADRDY;
    }
    return ++tick;
}

// Minimal vector table for this RAM model. QEMU zeroes RAM and loads the ELF.
extern char _stack_top;
void run_tests(void);
__attribute__((used, section(".test_vectors")))
const uintptr_t vectors[] = {(uintptr_t)&_stack_top, (uintptr_t)run_tests};

void run_tests(void) {
    uint32_t result[] = {0x20026U, test_adc()};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = result;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

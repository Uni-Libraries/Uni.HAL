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
#include <stm32l4xx_ll_adc.h>

// Hardware ignores writing zero to ADEN when LL writes the ADSTART command.
static void model_start_conversion(ADC_TypeDef *adc) {
    uint32_t const enabled = adc->CR & ADC_CR_ADEN;
    LL_ADC_REG_StartConversion(adc);
    adc->CR |= enabled;
}
#define LL_ADC_REG_StartConversion model_start_conversion

#define LL_ADC_ClearFlag_ADRDY(adc) ((adc)->ISR &= ~ADC_ISR_ADRDY) // W1C
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

#if defined(UNI_HAL_TARGET_MCU_STM32L496)
void (*adc_delay_hook)(void);
const DMA_TypeDef *adc_model_dma1(void) { return DMA1; }
#endif

bool adc_dma_init_succeeds;

unsigned dma_init_calls;
static uint32_t tick;
unsigned adc_calibration_delay, adc_ready_delay;
uint32_t adc_tick_step;

void reset_adc_model(void) {
    memset(adcs, 0, sizeof adcs);
    memset(dma_banks, 0, sizeof dma_banks);
    adc_dma_init_succeeds = false;
    memset(&common, 0, sizeof common);
    adc_peripheral_calls = 0;
    dma_init_calls = 0;
    tick = 0;
    adc_tick_step = 1U;
    adc_calibration_delay = adc_ready_delay = 0U;
}

bool adc_model_untouched(void) {
    const ADC_TypeDef reset_adc[3] = {};
    const ADC_Common_TypeDef reset_common = {};
    const dma_bank_t reset_dma[2] = {};
    return adc_peripheral_calls == 0U && memcmp(adcs, reset_adc, sizeof adcs) == 0 &&
           memcmp(&common, &reset_common, sizeof common) == 0 &&
           memcmp(dma_banks, reset_dma, sizeof dma_banks) == 0;
}

const ADC_TypeDef *adc_model_registers(uni_hal_core_periph_e instance) {
    return _uni_hal_adc_get_instance(instance);
}

ADC_Common_TypeDef *adc_model_common_registers(void) {
    return &common;
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

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
// TE clears EN in hardware. A direct-mode error can leave EN set until software stops the stream.
bool adc_model_dma_fault(bool direct_mode) {
    uint32_t const stream = LL_DMA_STREAM_1;
    uint32_t const flag = direct_mode ? DMA_LISR_DMEIF1 : DMA_LISR_TEIF1;
    DMA1->LISR |= flag | DMA_LISR_TCIF1;
    if (!direct_mode) {
        LL_DMA_DisableStream(DMA1, stream);
    }
    bool interrupt = direct_mode ? LL_DMA_IsEnabledIT_DME(DMA1, stream) : LL_DMA_IsEnabledIT_TE(DMA1, stream);
    DMA1->LIFCR = 0U;
    if (interrupt) {
        DMA1_Stream1_IRQHandler();
        DMA1->LISR &= ~DMA1->LIFCR; // W1C acknowledgement in the RAM model.
    }
    return !(DMA1->LISR & flag) && !LL_DMA_IsEnabledStream(DMA1, stream);
}
#endif

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
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
    if (adc_delay_hook) {
        void (*hook)(void) = adc_delay_hook;
        adc_delay_hook = nullptr;
        hook();
    }
#endif
}

uint32_t model_tick(void) {
    for (unsigned i = 0; i < 3; ++i) {
#if defined(UNI_HAL_TARGET_MCU_STM32L496)
        if (adcs[i].CR & ADC_CR_ADDIS) {
            adcs[i].CR &= ~(ADC_CR_ADEN | ADC_CR_ADDIS);
        }
#endif
        if (adcs[i].CR & ADC_CR_ADCAL) {
            if (adc_calibration_delay) {
                --adc_calibration_delay;
            } else {
                adcs[i].CR &= ~ADC_CR_ADCAL;
            }
        }
        if (adcs[i].CR & ADC_CR_ADEN) {
            if (adc_ready_delay) {
                --adc_ready_delay;
            } else {
                adcs[i].ISR |= ADC_ISR_ADRDY;
            }
        }
    }
    tick += adc_tick_step;
    return tick;
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

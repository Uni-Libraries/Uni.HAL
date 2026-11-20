// Use the firmware's vector table and STM32 LL channel accesses with both DMA blocks in RAM.
#include <math.h>
#include <stddef.h>
#include <stm32l4xx.h>

typedef struct {
    DMA_TypeDef control;
    struct {
        DMA_Channel_TypeDef regs;
        uint32_t reserved;
    } channel[7];
} dma_model_t;

static dma_model_t controllers[2];
static_assert(offsetof(dma_model_t, channel[6]) == DMA1_Channel7_BASE - DMA1_BASE);
#undef DMA1
#undef DMA2
#define DMA1 (&controllers[0].control)
#define DMA2 (&controllers[1].control)

// IFCR is write-only. Clearing GIF clears all three causes, and GIF reflects any remaining cause.
static void write_register(volatile uint32_t *reg, uint32_t value)
{
    for (unsigned module = 0; module < 2; ++module) {
        DMA_TypeDef *dma = &controllers[module].control;
        if (reg == &dma->IFCR) {
            for (unsigned shift = 0; shift < 28; shift += 4) {
                uint32_t causes = (dma->ISR >> shift) & 0xEU;
                uint32_t clear = value >> shift;
                causes = (clear & 1U) ? 0U : (causes & ~clear);
                dma->ISR = (dma->ISR & ~(0xFU << shift)) | ((causes ? causes | 1U : 0U) << shift);
            }
            return;
        }
        for (unsigned channel = 0; channel < 7; ++channel) {
            DMA_Channel_TypeDef *regs = &controllers[module].channel[channel].regs;
            if (reg == &regs->CNDTR && (regs->CCR & DMA_CCR_EN)) {
                return; // CNDTR ignores writes while the physical channel is enabled.
            }
        }
    }
    *reg = value;
}
#undef WRITE_REG
#define WRITE_REG(reg, value) write_register(&(reg), (value))
#include "dma/uni_hal_dma_stm32l4.c"
#include "adc/uni_hal_adc.h"

extern void (*const g_pfnVectors[])(void);
float uni_hal_adc_get_channel_voltage(const uni_hal_adc_context_t *ctx, uint32_t channel);
float uni_hal_adc_mcutemp_get(const uni_hal_adc_context_t *ctx);

static const IRQn_Type vectors[2][7] = {
    {DMA1_Channel1_IRQn, DMA1_Channel2_IRQn, DMA1_Channel3_IRQn, DMA1_Channel4_IRQn,
     DMA1_Channel5_IRQn, DMA1_Channel6_IRQn, DMA1_Channel7_IRQn},
    {DMA2_Channel1_IRQn, DMA2_Channel2_IRQn, DMA2_Channel3_IRQn, DMA2_Channel4_IRQn,
     DMA2_Channel5_IRQn, DMA2_Channel6_IRQn, DMA2_Channel7_IRQn}
};

bool __wrap_uni_hal_rcc_clk_set(uni_hal_core_periph_e periph, bool enabled)
{
    return enabled && (periph == UNI_HAL_CORE_PERIPH_DMA_1 || periph == UNI_HAL_CORE_PERIPH_DMA_2);
}

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static unsigned check_channel(unsigned module, unsigned channel)
{
    DMA_TypeDef *dma = &controllers[module].control;
    DMA_Channel_TypeDef *regs = &controllers[module].channel[channel].regs;
    unsigned const adjacent = (channel + 1U) % 7U;
    uint32_t const other_flags = (DMA_ISR_GIF1 | DMA_ISR_TEIF1) << (4U * adjacent);
    unsigned const shift = 4U * channel;
    uni_hal_dma_context_t ctx = {.config = {
        .instance = module ? UNI_HAL_CORE_PERIPH_DMA_2 : UNI_HAL_CORE_PERIPH_DMA_1,
        .channel = (uni_hal_dma_channel_e)(channel + 1U)
    }};
    uni_hal_dma_context_t neighbor = {.config = {.instance = ctx.config.instance,
        .channel = (uni_hal_dma_channel_e)(adjacent + 1U)}};
    uni_hal_dma_context_t opposite = {.config = {.instance = module ? UNI_HAL_CORE_PERIPH_DMA_1 : UNI_HAL_CORE_PERIPH_DMA_2,
        .channel = ctx.config.channel}};
    volatile uint16_t sample = 17U;
    uni_hal_adc_context_t adc = {.config = {
        .dma = &ctx, .data = &sample, .channels = {UNI_HAL_ADC_STM32L4_CHANNEL_REFINT}, .channels_count = 1U
    }, .state.initialized = true};
    void (*irq)(void) = g_pfnVectors[16 + vectors[module][channel]];
    CHECK(uni_hal_dma_init(&ctx));
    regs->CCR = DMA_CCR_CIRC | DMA_CCR_TEIE;
    CHECK(uni_hal_dma_enable(&ctx, true));
    CHECK(uni_hal_adc_get_rank_raw(&adc, 0) == sample);

    // Pending events and enable bits must survive on the adjacent channel and other controller.
    controllers[module].channel[adjacent].regs.CCR = DMA_CCR_TEIE;
    controllers[1U - module].channel[channel].regs.CCR = DMA_CCR_TEIE;
    CHECK(uni_hal_dma_init(&neighbor) && uni_hal_dma_enable(&neighbor, true));
    CHECK(uni_hal_dma_init(&opposite) && uni_hal_dma_enable(&opposite, true));
    controllers[1U - module].control.ISR = (DMA_ISR_GIF1 | DMA_ISR_TEIF1) << shift;
    dma->ISR = other_flags | (0xFU << shift); // TE, HT and TC together.
    irq();
    CHECK(dma->ISR == other_flags);
    CHECK(regs->CCR == (DMA_CCR_CIRC | DMA_CCR_TEIE));
    CHECK(controllers[module].channel[adjacent].regs.CCR == (DMA_CCR_EN | DMA_CCR_TEIE));
    CHECK(controllers[1U - module].channel[channel].regs.CCR == (DMA_CCR_EN | DMA_CCR_TEIE));
    CHECK(controllers[1U - module].control.ISR == ((DMA_ISR_GIF1 | DMA_ISR_TEIF1) << shift));
    CHECK(uni_hal_dma_stm32l4_has_transfer_error(&ctx));
    CHECK(!uni_hal_dma_stm32l4_has_transfer_error(&neighbor));
    CHECK(!uni_hal_dma_stm32l4_has_transfer_error(&opposite));
    CHECK(!uni_hal_adc_is_inited(&adc));
    CHECK(uni_hal_adc_get_rank_raw(&adc, 0) == UINT16_MAX);
    CHECK(uni_hal_adc_get_channel_raw(&adc, UNI_HAL_ADC_STM32L4_CHANNEL_REFINT) == UINT16_MAX);
    CHECK(uni_hal_adc_get_channel_mv(&adc, UNI_HAL_ADC_STM32L4_CHANNEL_REFINT) == UINT16_MAX);
    CHECK(isnan(uni_hal_adc_get_channel_voltage(&adc, UNI_HAL_ADC_STM32L4_CHANNEL_REFINT)));
    CHECK(uni_hal_adc_stm32l4_get_vdda(&adc) == UINT32_MAX);
    adc.config.channels[0] = UNI_HAL_ADC_STM32L4_CHANNEL_TEMPSENSOR;
    CHECK(uni_hal_adc_stm32l4_get_mcutemp(&adc) == INT32_MAX);
    CHECK(isnan(uni_hal_adc_mcutemp_get(&adc)));
    irq(); // An already-queued interrupt must preserve the error until a new transfer.
    CHECK(!uni_hal_adc_is_inited(&adc));

    // An old pending TE before restarting must not be attributed to the new transfer.
    dma->ISR |= (DMA_ISR_GIF1 | DMA_ISR_TEIF1) << shift;
    CHECK(uni_hal_dma_enable(&ctx, false));
    CHECK(uni_hal_dma_stm32l4_has_transfer_error(&ctx));
    CHECK(uni_hal_dma_set_length(&ctx, 0U) && regs->CNDTR == 0U);
    CHECK(uni_hal_dma_set_length(&ctx, UINT16_MAX) && regs->CNDTR == UINT16_MAX);
    CHECK(!uni_hal_dma_set_length(&ctx, (uint32_t)UINT16_MAX + 1U));
    CHECK(!uni_hal_dma_set_length(&ctx, UINT32_MAX));
    CHECK(regs->CNDTR == UINT16_MAX);
    CHECK(uni_hal_dma_set_length(&ctx, 4));
    CHECK(uni_hal_dma_enable(&ctx, true));
    irq();
    CHECK(!uni_hal_dma_stm32l4_has_transfer_error(&ctx));
    bool const changed = uni_hal_dma_set_length(&ctx, 9U);
    CHECK(regs->CNDTR == 4 && regs->CCR == (DMA_CCR_CIRC | DMA_CCR_TEIE | DMA_CCR_EN));
    CHECK(!changed);
    CHECK(dma->ISR == other_flags);
    CHECK(uni_hal_adc_get_rank_raw(&adc, 0) == sample);
    dma->ISR |= (DMA_ISR_GIF1 | DMA_ISR_HTIF1 | DMA_ISR_TCIF1) << shift;
    CHECK(uni_hal_dma_enable(&ctx, true)); // Enabling an active transfer must preserve its events.
    CHECK((dma->ISR & (DMA_ISR_TCIF1 << shift)) != 0U);
    irq();
    CHECK(dma->ISR == other_flags && (regs->CCR & DMA_CCR_EN));
    CHECK(uni_hal_adc_is_inited(&adc));

    // Real hardware has already cleared EN when it reports TE.
    regs->CCR &= ~DMA_CCR_EN;
    dma->ISR |= (DMA_ISR_GIF1 | DMA_ISR_TEIF1) << shift;
    irq();
    CHECK(dma->ISR == other_flags && !uni_hal_adc_is_inited(&adc));
    CHECK(uni_hal_dma_set_length(&ctx, 9U) && regs->CNDTR == 9U);
    return 0;
}

int main(void)
{
    unsigned error = 0;
    for (unsigned module = 0; module < 2 && !error; ++module) {
        for (unsigned channel = 0; channel < 7 && !error; ++channel) {
            error = check_channel(module, channel);
        }
    }
    uint32_t block[] = {0x20026U, error};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = block;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

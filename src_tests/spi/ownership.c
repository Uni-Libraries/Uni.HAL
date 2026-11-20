// Exercise the H743 drivers and the STM32 LL on Cortex-M7 with peripherals in SRAM.
#include <stm32h7xx.h>
#include <stdbool.h>
#include <stddef.h>

static SPI_TypeDef spi;
static ADC_TypeDef adc_regs[3];
static ADC_Common_TypeDef adc_common;
static DBGMCU_TypeDef debug;
#undef ADC1
#undef ADC2
#undef ADC3
#undef ADC12_COMMON
#undef DBGMCU
#define ADC1 (&adc_regs[0])
#define ADC2 (&adc_regs[1])
#define ADC3 (&adc_regs[2])
#define ADC12_COMMON (&adc_common)
#define DBGMCU (&debug)
static unsigned exception_number;
#define __get_IPSR() exception_number
static bool hold_suspend;
struct dma_model {
    DMA_TypeDef regs;
    DMA_Stream_TypeDef stream[8];
};
static struct dma_model controllers[2];
static DMAMUX_Channel_TypeDef mux[16];
static_assert(offsetof(struct dma_model, stream[7]) == DMA1_Stream7_BASE - DMA1_BASE);
static volatile uint32_t *slow_disable;
#undef SPI1
#undef DMA1
#undef DMA2
#undef DMAMUX1_Channel0
#define SPI1 (&spi)
#define DMA1 (&controllers[0].regs)
#define DMA2 (&controllers[1].regs)
#define DMAMUX1_Channel0 mux

static unsigned spi_pending;
#undef NVIC_EnableIRQ
#undef NVIC_SetPriority
#undef NVIC_SetPendingIRQ
#define NVIC_EnableIRQ(irq) ((void)(irq))
#define NVIC_SetPriority(irq, priority) ((void)(irq), (void)(priority))
#define NVIC_SetPendingIRQ(irq) ((void)(irq), ++spi_pending)

static void write_reg(volatile uint32_t *addr, uint32_t val) {
    if (addr == &spi.IFCR) {
        spi.SR &= ~val;
        return;
    }
    for (unsigned m = 0; m < 2; ++m) {
        DMA_TypeDef *regs = &controllers[m].regs;
        if (addr == &regs->LIFCR) { regs->LISR &= ~val; return; }
        if (addr == &regs->HIFCR) { regs->HISR &= ~val; return; }
    }
    if (addr == slow_disable && (*addr & DMA_SxCR_EN)) {
        val |= DMA_SxCR_EN; // Model an outstanding DMA bus beat after a software disable.
    }
    if (addr == &spi.CR1 && (val & SPI_CR1_CSUSP) && !hold_suspend) {
        val &= ~(SPI_CR1_CSTART | SPI_CR1_CSUSP);
        spi.SR |= SPI_SR_SUSP;
    }
    if (addr == &spi.CR1 && !(val & SPI_CR1_SPE)) {
        val &= ~SPI_CR1_CSTART;
    }
    *addr = val;
}
#undef SET_BIT
#undef WRITE_REG
#undef CLEAR_BIT
#define WRITE_REG(reg, val) write_reg(&(reg), (val))
#define SET_BIT(reg, bits) WRITE_REG(reg, (reg) | (bits))
#define CLEAR_BIT(reg, bits) WRITE_REG(reg, (reg) & ~(bits))

#define uni_hal_rcc_clk_set model_clock
#define uni_hal_rcc_clksrc_set model_source
#define uni_hal_gpio_pin_set model_nss
#define uni_hal_systick_get_ms model_tick
#define uni_hal_core_cm7_dcache_invalidate model_invalidate
#define uni_hal_core_cm7_dcache_clean model_clean
#define uni_hal_core_cm7_dcache_cleaninvalidate_addr model_cleaninvalidate
#include "dma/uni_hal_dma_stm32h7.c"
#include "spi/uni_hal_spi_stm32h7.c"
#include "adc/uni_hal_adc_stm32h7.c"

static bool fail_adc_clock;
static void (*adc_clock_hook)(void);
bool model_clock(uni_hal_core_periph_e instance, bool enable) {
    if (instance == UNI_HAL_CORE_PERIPH_ADC_1 && adc_clock_hook) {
        adc_clock_hook();
    }
    return enable && !(instance == UNI_HAL_CORE_PERIPH_ADC_1 && fail_adc_clock);
}
// The CPU can enqueue and drain a byte before its final CRC or error status arrives.
static uint32_t frame_status[] = {SPI_SR_TXP, SPI_SR_RXP, 0U};
static size_t frame_step = 3U;
static void frame_ending_with(uint32_t status) {
    frame_status[2] = status;
    frame_step = 0U;
}

uint32_t model_tick(void) {
    static uint32_t tick;
    if (frame_step < 3U) {
        spi.SR = frame_status[frame_step++];
    }
    ADC1->CR &= ~ADC_CR_ADCAL;
    if (ADC1->CR & ADC_CR_ADEN) {
        ADC1->ISR |= ADC_ISR_ADRDY;
    }
    return ++tick;
}
bool model_source(uni_hal_core_periph_e instance, uni_hal_rcc_clksrc_e source) {
    (void)instance; (void)source; return true;
}
static bool nss;
bool model_nss(uni_hal_gpio_pin_context_t *pin, bool high) { (void)pin; nss = high; return true; }
static unsigned invalidations;
static void (*cache_hook)(void);
void model_invalidate(void *ptr, int32_t bytes) { (void)ptr; (void)bytes; ++invalidations; if (cache_hook) cache_hook(); }
void model_clean(void *ptr, int32_t bytes) { (void)ptr; (void)bytes; if (cache_hook) cache_hook(); }
void model_cleaninvalidate(void *ptr, int32_t bytes) { (void)ptr; (void)bytes; }

static uint8_t rx[32], tx[32];
static uni_hal_dma_context_t rx_dma = {.config = {UNI_HAL_CORE_PERIPH_DMA_1, UNI_HAL_DMA_CHANNEL_0}};
static uni_hal_dma_context_t tx_dma = {.config = {UNI_HAL_CORE_PERIPH_DMA_2, UNI_HAL_DMA_CHANNEL_7}};
static uni_hal_spi_context_t ctx = {.config = {
    .instance = UNI_HAL_CORE_PERIPH_SPI_1, .dma_rx = &rx_dma, .dma_tx = &tx_dma
}};
#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (false)

static unsigned callbacks, last_error;
static bool restart, reentered, callback_ok;
static uint16_t adc_sample;
static uni_hal_adc_context_t adc = {.config = {.instance = UNI_HAL_CORE_PERIPH_ADC_1,
    .clock_source = UNI_HAL_RCC_CLKSRC_PLL2P, .dma = &tx_dma, .data = &adc_sample, .channels_count = 1}};


static bool adc_setup_owned;
static void try_interrupting_adc_setup(void) {
    adc_clock_hook = nullptr;
    adc_setup_owned = !uni_hal_dma_enable(&tx_dma, false) && !uni_hal_spi_transceive_async(&ctx, nullptr, tx, 1);
}

static bool completed(void *cookie) {
    ++callbacks;
    callback_ok = cookie == &ctx && !uni_hal_spi_is_busy(&ctx) && nss && !LL_SPI_IsEnabled(SPI1) &&
                  ctx.status.last_rx_data == nullptr && !LL_DMA_IsEnabledStream(DMA1, 0) &&
                  !LL_DMA_IsEnabledStream(DMA2, 7);
    ++exception_number; // A higher-priority producer preempts this SPI callback.
    callback_ok &= !uni_hal_spi_transceive_async(&ctx, nullptr, tx, sizeof tx);
    --exception_number;
    last_error = uni_hal_spi_get_last_errors(&ctx);
    if (restart) {
        restart = false;
        callback_ok &= uni_hal_spi_transceive_async(&ctx, nullptr, tx, sizeof tx);
    }
    return false;
}

static void try_reentry(void) {
    cache_hook = nullptr;
    reentered = uni_hal_spi_is_busy(&ctx) &&
                !uni_hal_spi_transceive_async(&ctx, rx + 1, nullptr, 3) &&
                !uni_hal_spi_transmit(&ctx, tx, 3) && !uni_hal_spi_abort(&ctx);
    // A stale SPI vector during preparation or cache handover cannot consume this transfer.
    SPI1_IRQHandler();
    reentered &= !uni_hal_adc_init(&adc);
}

static void tx_done(void) {
    controllers[1].stream[7].NDTR = 0;
    controllers[1].stream[7].CR &= ~DMA_SxCR_EN;
    DMA2->HISR |= DMA_HISR_TCIF7;
    DMA2_Stream7_IRQHandler();
}

static void rx_done(void) {
    controllers[0].stream[0].NDTR = 0;
    controllers[0].stream[0].CR &= ~DMA_SxCR_EN;
    DMA1->LISR |= DMA_LISR_TCIF0;
    DMA1_Stream0_IRQHandler();
}

__attribute__((noinline)) static unsigned check_spi(void) {
    SPI1_IRQHandler(); // Early vector, before an owner has registered.
    CHECK(uni_hal_spi_init(&ctx));
    CHECK(uni_hal_spi_set_callback(&ctx, completed, &ctx));
    uni_hal_spi_context_t other = {.config = ctx.config};
    other.config.instance = UNI_HAL_CORE_PERIPH_SPI_6;
    CHECK(!uni_hal_spi_init(&other)); // SPI6 requests are routed to BDMA.

    CHECK(!uni_hal_spi_transceive_async(&ctx, rx, tx, 0));
    CHECK(!uni_hal_spi_transceive_async(&ctx, rx, tx, UINT16_MAX + 1U));
    CHECK(!uni_hal_spi_transmitreceive(&ctx, tx, rx, UINT16_MAX + 1U));
    CHECK(!uni_hal_spi_transceive_async(&ctx, nullptr, nullptr, 32));
    CHECK(!uni_hal_spi_transceive_async(&ctx, rx, rx + 1, 8));
    CHECK(!uni_hal_spi_transceive_async(&ctx, rx + 1, rx, 8));
    CHECK(!uni_hal_spi_is_busy(&ctx) && !spi.CR2 && !invalidations);

    cache_hook = try_reentry;
    CHECK(uni_hal_spi_transceive_async(&ctx, rx, tx, sizeof tx));
    CHECK(reentered && uni_hal_spi_is_busy(&ctx) && !nss);
    other.config.instance = ctx.config.instance;
    CHECK(!uni_hal_spi_init(&other));
    CHECK(!uni_hal_dma_enable(&tx_dma, false) && !uni_hal_dma_set_priority(&tx_dma, UNI_HAL_DMA_PRIORITY_HIGH));
    CHECK(controllers[0].stream[0].M0AR == (uint32_t)rx && controllers[1].stream[7].M0AR == (uint32_t)tx);
    CHECK(LL_SPI_GetTransferDirection(SPI1) == LL_SPI_FULL_DUPLEX);
    CHECK(!uni_hal_spi_transceive_async(&ctx, nullptr, tx + 1, 31));
    CHECK(!uni_hal_spi_receive(&ctx, rx, 31));
    CHECK(!uni_hal_spi_transmitreceive(&ctx, tx, rx, 31));
    CHECK(!uni_hal_spi_set_prescaler(&ctx, UNI_HAL_SPI_PRESCALER_8));
    CHECK(spi.CR2 == sizeof tx && ctx.status.last_rx_data == rx && !nss);

    spi.SR |= SPI_SR_TXP | SPI_SR_TXC;
    SPI1_IRQHandler();
    CHECK(uni_hal_spi_is_busy(&ctx) && !callbacks && !invalidations);
    // EOT can precede the last RX write. TX has finished, but RX and its cache lines are still borrowed.
    spi.SR |= SPI_SR_EOT;
    tx_done();
    SPI1_IRQHandler();
    CHECK(uni_hal_spi_is_busy(&ctx) && !callbacks && !invalidations && !nss);
    rx_done();
    CHECK(!uni_hal_adc_init(&adc)); // Both DMA streams have cleared EN but SPI still owns them until EOT.
    CHECK(LL_SPI_IsEnabledIT_EOT(SPI1));
    restart = true;
    cache_hook = try_reentry;
    SPI1_IRQHandler();
    CHECK(callbacks == 1 && callback_ok && reentered && !last_error && invalidations == 1);
    CHECK(uni_hal_spi_is_busy(&ctx) && !nss && LL_SPI_GetTransferDirection(SPI1) == LL_SPI_SIMPLEX_TX);
    CHECK(!LL_SPI_IsEnabledDMAReq_RX(SPI1) && LL_SPI_IsEnabledDMAReq_TX(SPI1));
    SPI1_IRQHandler();
    CHECK(callbacks == 1 && uni_hal_spi_is_busy(&ctx));
    tx_done();
    CHECK(!uni_hal_adc_init(&adc));
    SPI1_IRQHandler();
    CHECK(callbacks == 1); // DMA TC alone does not assert chip select or complete the wire transfer.
    spi.SR |= SPI_SR_EOT | SPI_SR_CRCE;
    SPI1_IRQHandler();
    CHECK(callbacks == 2 && callback_ok && !last_error && nss && invalidations == 1);
    SPI1_IRQHandler();
    CHECK(callbacks == 2 && !uni_hal_spi_abort(&ctx));

    // An RX-only transfer does not claim the configured TX stream.
    controllers[1].stream[7].CR = DMA_SxCR_EN;
    CHECK(uni_hal_spi_transceive_async(&ctx, rx, nullptr, sizeof rx));
    CHECK(LL_SPI_GetTransferDirection(SPI1) == LL_SPI_SIMPLEX_RX && !LL_SPI_IsEnabledDMAReq_TX(SPI1));
    CHECK(uni_hal_spi_abort(&ctx));
    CHECK(controllers[1].stream[7].CR == DMA_SxCR_EN && callbacks == 2 && invalidations == 2 && nss);
    controllers[1].stream[7].CR = 0;

    // An abort may wait for the last DMA bus beat. No callback, reuse or invalidation before EN clears.
    CHECK(uni_hal_spi_transceive_async(&ctx, rx, tx, sizeof rx));
    slow_disable = &controllers[0].stream[0].CR;
    CHECK(!uni_hal_spi_abort(&ctx) && !uni_hal_spi_abort(&ctx));
    CHECK(uni_hal_spi_is_busy(&ctx) && invalidations == 2 && !nss);
    CHECK(!uni_hal_spi_transceive_async(&ctx, nullptr, tx, 1));
    slow_disable = nullptr;
    rx_done();
    CHECK(spi_pending);
    SPI1_IRQHandler();
    CHECK(!uni_hal_spi_is_busy(&ctx) && callbacks == 2 && invalidations == 3 && nss);
    SPI1_IRQHandler();
    CHECK(callbacks == 2);

    // Combined DMA/SPI errors, without EOT. Callback can restart immediately.
    CHECK(uni_hal_spi_transceive_async(&ctx, rx, tx, sizeof rx));
    DMA2->HISR |= DMA_HISR_TEIF7 | DMA_HISR_TCIF7 | DMA_HISR_TCIF6;
    DMA2_Stream7_IRQHandler();
    CHECK(DMA2->HISR == DMA_HISR_TCIF6);
    spi.SR |= SPI_SR_CRCE | SPI_SR_OVR | SPI_SR_UDR | SPI_SR_MODF | SPI_SR_TIFRE;
    SPI1_IRQHandler();
    CHECK(callbacks == 3 && callback_ok && invalidations == 4);
    CHECK(last_error == (UNI_HAL_SPI_ERROR_DMA | UNI_HAL_SPI_ERROR_CRC | UNI_HAL_SPI_ERROR_OVERRUN |
                         UNI_HAL_SPI_ERROR_UNDERRUN | UNI_HAL_SPI_ERROR_MODE_FAULT | UNI_HAL_SPI_ERROR_FRAME));

    CHECK(uni_hal_spi_transceive_async(&ctx, rx, nullptr, sizeof rx));
    DMA1->LISR |= DMA_LISR_DMEIF0;
    DMA1_Stream0_IRQHandler();
    SPI1_IRQHandler();
    CHECK(callbacks == 4 && callback_ok && last_error == UNI_HAL_SPI_ERROR_DMA && invalidations == 5);

    CHECK(uni_hal_spi_transceive_async(&ctx, rx, rx, sizeof rx));
    CHECK(controllers[0].stream[0].M0AR == (uintptr_t)rx && controllers[1].stream[7].M0AR == (uintptr_t)rx);
    CHECK(uni_hal_spi_abort(&ctx));
    // Independent TX, with no RX descriptor at all. Exercise the 16-bit hardware limit.
    ctx.config.dma_rx = nullptr;
    static const uint8_t large[UINT16_MAX] = {};
    CHECK(!uni_hal_spi_transceive_async(&ctx, rx, nullptr, 1));
    CHECK(uni_hal_spi_transceive_async(&ctx, nullptr, large, sizeof large));
    CHECK(spi.CR2 == UINT16_MAX && controllers[1].stream[7].NDTR == UINT16_MAX);
    CHECK(uni_hal_spi_abort(&ctx) && callbacks == 4 && nss);
    // If suspension takes longer, the first abort retains both buffers until SUSP; a retry can force it.
    hold_suspend = true;
    CHECK(uni_hal_spi_transceive_async(&ctx, nullptr, tx, 1));
    CHECK(!uni_hal_spi_abort(&ctx) && uni_hal_spi_is_busy(&ctx) && !nss);
    SPI1_IRQHandler();
    CHECK(uni_hal_spi_is_busy(&ctx) && !nss);
    spi.CR1 &= ~(SPI_CR1_CSTART | SPI_CR1_CSUSP);
    spi.SR |= SPI_SR_SUSP;
    SPI1_IRQHandler();
    CHECK(!uni_hal_spi_is_busy(&ctx) && callbacks == 4 && nss);
    CHECK(uni_hal_spi_transceive_async(&ctx, nullptr, tx, 1));
    CHECK(!uni_hal_spi_abort(&ctx) && uni_hal_spi_abort(&ctx));
    hold_suspend = false;

    // A failed ADC initialization gives the stream back. A successful circular capture keeps it.
    CHECK((controllers[1].stream[7].CR & (DMA_SxCR_TCIE | DMA_SxCR_DMEIE | DMA_SxCR_TEIE)) == 0U);
    fail_adc_clock = true;
    CHECK(!uni_hal_adc_init(&adc));
    CHECK(uni_hal_spi_transceive_async(&ctx, nullptr, tx, 1) && uni_hal_spi_abort(&ctx));
    fail_adc_clock = false;
    adc_clock_hook = try_interrupting_adc_setup;
    CHECK(uni_hal_adc_init(&adc) && adc_setup_owned && uni_hal_adc_init(&adc));
    CHECK(!uni_hal_spi_transceive_async(&ctx, nullptr, tx, 1));
    uni_hal_dma_context_t alias = tx_dma;
    CHECK(!uni_hal_dma_enable(&alias, false) && !uni_hal_dma_set_fifo_mode(&alias, true));
    CHECK(controllers[1].stream[7].M0AR == (uintptr_t)&adc_sample && controllers[1].stream[7].NDTR == 1);
    CHECK(controllers[1].stream[7].CR & DMA_SxCR_EN);
    CHECK(uni_hal_dma_enable(&tx_dma, false));

    other.config.mode = UNI_HAL_SPI_MODE_SLAVE;
    other.config.crc_type = UNI_HAL_SPI_CRC_16BIT;
    other.config.crc_polynomial = 0x1021U;
    CHECK(uni_hal_spi_init(&other) && !uni_hal_spi_is_inited(&ctx));
    CHECK(!uni_hal_spi_transceive_async(&ctx, nullptr, tx, 1));
    CHECK(uni_hal_spi_transceive_async(&other, nullptr, tx, 1));
    CHECK(LL_SPI_GetTransferDirection(SPI1) == LL_SPI_SIMPLEX_TX && !(spi.CR1 & SPI_CR1_CSTART));
    CHECK(LL_SPI_GetInternalSSLevel(SPI1) == LL_SPI_SS_LEVEL_LOW);
    CHECK(uni_hal_spi_abort(&other));

    // A completed wire transfer with an invalid CRC is a failed blocking call.
    frame_ending_with(SPI_SR_EOT | SPI_SR_CRCE);
    CHECK(!uni_hal_spi_transmitreceive(&other, tx, rx, 1U));
    CHECK(!uni_hal_spi_is_busy(&other) && !LL_SPI_IsEnabled(SPI1) && !(spi.SR & SPI_SR_CRCE));

    // After a slave loses a byte, no further clock or RTOS timeout is needed to release it.
    frame_ending_with(SPI_SR_OVR);
    CHECK(!uni_hal_spi_transmit(&other, tx, sizeof tx));
    CHECK(!uni_hal_spi_is_busy(&other) && nss && !LL_SPI_IsEnabled(SPI1) && !(spi.SR & SPI_SR_OVR));

    frame_ending_with(SPI_SR_EOT);
    CHECK(uni_hal_spi_transmit(&other, tx, 1U));
    CHECK(nss && !uni_hal_spi_is_busy(&other));
    return 0;
}

extern char _stack_top;
void run_tests(void);
__attribute__((used, section(".test_vectors")))
const uintptr_t vectors[] = {(uintptr_t)&_stack_top, (uintptr_t)run_tests};
void run_tests(void) {
    // This standalone RAM image bypasses SystemInit(), including its Cortex-M7 FPU setup.
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    __DSB();
    __ISB();
    uint32_t result[] = {0x20026U, check_spi()};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = result;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

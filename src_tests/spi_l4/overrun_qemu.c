#include <stm32l4xx_ll_spi.h>
#include "../arm/semihost_test.h"

static SPI_TypeDef spi;
#undef SPI1
#define SPI1 (&spi)

// Four received bytes fit in the L496 FIFO. The fifth is lost and latches OVR.
static uint8_t fifo[4], head, count;
static bool burst;
static unsigned next_byte;

static void update_rx_status(void) {
    uint32_t level = LL_SPI_RX_FIFO_EMPTY;
    if (count > 2) {
        level = LL_SPI_RX_FIFO_FULL;
    } else if (count == 2) {
        level = LL_SPI_RX_FIFO_HALF_FULL;
    } else if (count == 1) {
        level = LL_SPI_RX_FIFO_QUARTER_FULL;
    }
    MODIFY_REG(spi.SR, SPI_SR_FRLVL | SPI_SR_RXNE, level | (count ? SPI_SR_RXNE : 0U));
}

static void receive_frame(void) {
    if (count == sizeof(fifo)) {
        spi.SR |= SPI_SR_OVR;
    } else {
        fifo[(head + count) % sizeof(fifo)] = (uint8_t)next_byte;
        ++count;
        update_rx_status();
    }
    ++next_byte;
}

static void clock_frames(SPI_TypeDef *instance, uint8_t byte) {
    LL_SPI_TransmitData8(instance, byte);
    unsigned frames = burst ? 5 : 1;
    burst = false;
    while (frames--) {
        receive_frame();
    }
}

static uint8_t read_byte(SPI_TypeDef *instance) {
    (void)instance;
    if (!count) {
        return 0U;
    }
    uint8_t const data = fifo[head];
    head = (head + 1U) % sizeof(fifo);
    --count;
    update_rx_status();
    return data;
}

static void clear_overrun(SPI_TypeDef *instance) {
    (void)read_byte(instance);
    instance->SR &= ~SPI_SR_OVR;
}

#define LL_SPI_ReceiveData8 read_byte
#define LL_SPI_TransmitData8 clock_frames
#define LL_SPI_ClearFlag_OVR clear_overrun
#include "spi/uni_hal_spi_stm32l4.c"

uint32_t uni_hal_systick_get_ms(void) { return 0U; }

static unsigned check(void) {
    uni_hal_spi_context_t ctx = {
        .config = {.instance = UNI_HAL_CORE_PERIPH_SPI_1, .mode = UNI_HAL_SPI_MODE_SLAVE},
        .status.inited = true,
    };
    uint8_t data[5] = {0x77};
    spi.SR = SPI_SR_TXE;
    burst = true;
    CHECK(!uni_hal_spi_receive(&ctx, data, sizeof(data)));
    CHECK(data[0] == 0x77 && count == 0U && !LL_SPI_IsEnabled(SPI1));
    CHECK(uni_hal_spi_receive(&ctx, data, 1U) && data[0] == 5U);
    CHECK(count == 0U && !LL_SPI_IsEnabled(SPI1));
    return 0;
}

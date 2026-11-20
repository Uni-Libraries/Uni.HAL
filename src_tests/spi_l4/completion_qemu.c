#include <stm32l4xx_ll_spi.h>

#include "../arm/semihost_test.h"

static SPI_TypeDef spi;
#undef SPI1
#define SPI1 (&spi)

// Model an external clock delivering a byte on the second RXNE poll.
static unsigned rx_polls;
static bool awaiting_rx;
static void transmit(SPI_TypeDef *instance, uint8_t byte) {
    LL_SPI_TransmitData8(instance, byte);
    instance->SR &= ~SPI_SR_RXNE;
    awaiting_rx = true;
}

static uint32_t model_status(SPI_TypeDef *instance) {
    if (awaiting_rx && ++rx_polls % 2U == 0U) {
        instance->DR ^= UINT8_MAX;
        instance->SR |= SPI_SR_RXNE;
        awaiting_rx = false;
    }
    return instance->SR;
}
#undef LL_SPI_ReadReg
#define LL_SPI_ReadReg(instance, reg) model_status(instance)
#define LL_SPI_TransmitData8 transmit
#include "spi/uni_hal_spi_stm32l4.c"

static uint32_t elapsed_ms;
static uint32_t busy_until_ms;
uint32_t uni_hal_systick_get_ms(void) {
    ++elapsed_ms;
    if (elapsed_ms == busy_until_ms) {
        spi.SR &= ~SPI_SR_BSY;
    }
    return elapsed_ms;
}

static unsigned check(void) {
    uni_hal_spi_context_t ctx = {
        .config = {.instance = UNI_HAL_CORE_PERIPH_SPI_1, .mode = UNI_HAL_SPI_MODE_SLAVE},
        .status.inited = true,
    };
    uint8_t const tx[] = {0x3C, 0x5A};
    uint8_t rx[2] = {};
    spi.SR = SPI_SR_TXE | SPI_SR_BSY;
    CHECK(uni_hal_spi_transmitreceive(&ctx, tx, rx, sizeof(tx)));
    CHECK(rx_polls == 4U && rx[0] == 0xC3 && rx[1] == 0xA5);
    CHECK((spi.SR & SPI_SR_BSY) && !(spi.CR1 & SPI_CR1_SPE));

    ctx.config.mode = UNI_HAL_SPI_MODE_MASTER;
    busy_until_ms = elapsed_ms + 20U;
    CHECK(uni_hal_spi_transmit(&ctx, tx, sizeof(tx)));
    CHECK(!(spi.SR & SPI_SR_BSY) && !(spi.CR1 & SPI_CR1_SPE));

    spi.SR |= SPI_SR_BSY;
    CHECK(!uni_hal_spi_receive(&ctx, rx, sizeof(rx)));
    CHECK(!(spi.CR1 & SPI_CR1_SPE));
    return 0;
}

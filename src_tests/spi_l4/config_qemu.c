// Run the L496 SPI driver and ST LL initialization against register banks in RAM.
#include <stm32l4xx.h>

static SPI_TypeDef regs[3];
#undef SPI1
#undef SPI2
#undef SPI3
#define SPI1 (&regs[0])
#define SPI2 (&regs[1])
#define SPI3 (&regs[2])

#include <stm32l4xx_ll_spi.h>

// Supply slave clocks after a simulated pause longer than the master timeout.
static unsigned delayed_txe_polls;
static uint32_t tick;
static uint32_t model_status(SPI_TypeDef *spi) {
    if (delayed_txe_polls) {
        ++tick;
        if (--delayed_txe_polls == 0U) {
            spi->SR |= SPI_SR_TXE | SPI_SR_RXNE;
        }
    }
    return spi->SR;
}

#undef LL_SPI_ReadReg
#define LL_SPI_ReadReg(instance, reg) model_status(instance)
#define uni_hal_rcc_clk_set model_clock_set
#define LL_RCC_GetSystemClocksFreq model_clock_freq
#include "spi/uni_hal_spi_stm32l4.c"
#include "../arm/semihost_test.h"

static uni_hal_gpio_pin_context_t sck, miso, mosi, nss;
static unsigned clock_calls, gpio_calls, nss_calls;
static bool clock_ok = true;

bool uni_hal_rcc_clk_set(uni_hal_core_periph_e instance, bool enabled) {
    ++clock_calls;
    return clock_ok && enabled && (instance == UNI_HAL_CORE_PERIPH_SPI_1 ||
        instance == UNI_HAL_CORE_PERIPH_SPI_2 || instance == UNI_HAL_CORE_PERIPH_SPI_3);
}

bool uni_hal_gpio_pin_init(uni_hal_gpio_pin_context_t *pin) {
    ++gpio_calls;
    if (pin == &nss) {
        ++nss_calls;
    }
    return pin != nullptr;
}

void LL_RCC_GetSystemClocksFreq(LL_RCC_ClocksTypeDef *clocks) {
    clocks->PCLK1_Frequency = 24000000U;
    clocks->PCLK2_Frequency = 80000000U;
}

uint32_t uni_hal_systick_get_ms(void) {
    return tick++;
}

static bool rejected(uni_hal_spi_config_t config) {
    uni_hal_spi_context_t ctx = {.config = config};
    unsigned clocks_before = clock_calls, pins_before = gpio_calls;
    uint32_t cr1 = SPI1->CR1, cr2 = SPI1->CR2;
    return !uni_hal_spi_init(&ctx) && !uni_hal_spi_is_inited(&ctx) &&
           clock_calls == clocks_before && gpio_calls == pins_before &&
           SPI1->CR1 == cr1 && SPI1->CR2 == cr2;
}

static unsigned check(void) {
    uni_hal_spi_config_t base = {
        .instance = UNI_HAL_CORE_PERIPH_SPI_1, .pin_sck = &sck, .pin_miso = &miso, .pin_mosi = &mosi,
    };

    static const uint32_t rates[] = {
        LL_SPI_BAUDRATEPRESCALER_DIV2, LL_SPI_BAUDRATEPRESCALER_DIV4, LL_SPI_BAUDRATEPRESCALER_DIV8,
        LL_SPI_BAUDRATEPRESCALER_DIV16, LL_SPI_BAUDRATEPRESCALER_DIV32, LL_SPI_BAUDRATEPRESCALER_DIV64,
        LL_SPI_BAUDRATEPRESCALER_DIV128, LL_SPI_BAUDRATEPRESCALER_DIV256,
    };
    for (unsigned i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {
        uni_hal_spi_context_t master = {.config = base};
        master.config.prescaler = (uni_hal_spi_prescaler_e)i;
        CHECK(uni_hal_spi_init(&master));
        CHECK(LL_SPI_GetBaudRatePrescaler(SPI1) == rates[i]);
        CHECK(uni_hal_spi_bitrate_get(&master) == 80000000U / (2U << i));
        CHECK((SPI1->CR1 & (SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI)) ==
              (SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI));
        CHECK((SPI1->CR2 & SPI_CR2_SSOE) == 0U);
    }

    uni_hal_spi_context_t hard = {.config = base};
    hard.config.instance = UNI_HAL_CORE_PERIPH_SPI_2;
    hard.config.clock_source = UNI_HAL_RCC_CLKSRC_PCLK1;
    hard.config.prescaler = UNI_HAL_SPI_PRESCALER_64;
    hard.config.nss_hard = true;
    hard.config.pin_nss = &nss;
    hard.config.polarity = UNI_HAL_SPI_CPOL_1;
    hard.config.phase = UNI_HAL_SPI_CPHA_1;
    CHECK(uni_hal_spi_init(&hard));
    CHECK(nss_calls == 1U);
    CHECK((SPI2->CR1 & (SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_CPHA | SPI_CR1_CPOL)) ==
          (SPI_CR1_MSTR | SPI_CR1_CPHA | SPI_CR1_CPOL));
    CHECK((SPI2->CR2 & SPI_CR2_SSOE) && !(SPI2->CR2 & SPI_CR2_NSSP));
    CHECK(uni_hal_spi_bitrate_get(&hard) == 24000000U / 64U);
    uint8_t tx = 0xA5U, rx = 0U;
    CHECK(!uni_hal_spi_transmit(&hard, &tx, 1U)); // master with TXE stuck low
    CHECK(tick > 100U && !LL_SPI_IsEnabled(SPI2));

    uni_hal_spi_context_t slave = {.config = hard.config};
    slave.config.mode = UNI_HAL_SPI_MODE_SLAVE;
    CHECK(uni_hal_spi_init(&slave));
    CHECK((SPI2->CR1 & (SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI)) == 0U);
    CHECK((SPI2->CR2 & SPI_CR2_SSOE) == 0U && nss_calls == 2U);
    CHECK(uni_hal_spi_bitrate_get(&slave) == 0U);
    // The register model loops writes to DR back to the reader.
    SPI2->SR = 0U;
    delayed_txe_polls = 200U;
    CHECK(uni_hal_spi_transmitreceive(&slave, &tx, &rx, 1U) && rx == tx);
    CHECK(delayed_txe_polls == 0U);
    slave.config.nss_hard = false;
    slave.config.pin_nss = nullptr;
    slave.status.inited = false;
    CHECK(uni_hal_spi_init(&slave));
    CHECK((SPI2->CR1 & (SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI)) == SPI_CR1_SSM);

    uni_hal_spi_config_t bad = base;
    bad.mode = (uni_hal_spi_mode_e)2;
    CHECK(rejected(bad));
    bad = base;
    bad.prescaler = (uni_hal_spi_prescaler_e)8;
    CHECK(rejected(bad));
    bad = base;
    bad.phase = (uni_hal_spi_cpha_e)2;
    CHECK(rejected(bad));
    bad = base;
    bad.polarity = (uni_hal_spi_cpol_e)2;
    CHECK(rejected(bad));
    bad = base;
    bad.nss_hard = true;
    CHECK(rejected(bad));
    bad = base;
    bad.pin_sck = nullptr;
    CHECK(rejected(bad));
    bad = base;
    bad.clock_source = UNI_HAL_RCC_CLKSRC_PCLK1; // SPI1 uses PCLK2.
    CHECK(rejected(bad));
    bad = base;
    bad.crc_type = UNI_HAL_SPI_CRC_16BIT;
    CHECK(rejected(bad));
    uni_hal_dma_context_t dma = {};
    bad = base;
    bad.dma_tx = &dma;
    CHECK(rejected(bad));
    bad.dma_tx = nullptr;
    bad.dma_rx = &dma;
    CHECK(rejected(bad));

    clock_ok = false;
    unsigned pins_before = gpio_calls;
    uint32_t cr1 = SPI1->CR1;
    uni_hal_spi_context_t failed = {.config = base};
    CHECK(!uni_hal_spi_init(&failed) && !uni_hal_spi_is_inited(&failed));
    CHECK(gpio_calls == pins_before && SPI1->CR1 == cr1);
    return 0;
}

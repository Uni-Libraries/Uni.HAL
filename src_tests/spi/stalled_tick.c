#include "spi/uni_hal_spi.h"
#include "../arm/semihost_test.h"

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_spi.h>
#else
#include <stm32l4xx_ll_spi.h>
#endif

static SPI_TypeDef spi;
#undef SPI1
#define SPI1 (&spi)

static unsigned polls;
static uint32_t ready_status(void) {
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    return SPI_SR_TXP | SPI_SR_RXP | SPI_SR_EOT;
#else
    return SPI_SR_TXE | SPI_SR_RXNE;
#endif
}

static uint32_t frozen_tick(void) {
    // An eventual recovery lets the broken, unbounded loop return a visible false success.
    if (++polls == 16000U) {
        spi.SR = ready_status();
    }
    return 0;
}

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
static bool nss_high;
static unsigned nss_edges;
static bool set_nss(uni_hal_gpio_pin_context_t *pin, bool high) {
    (void)pin;
    nss_high = high;
    ++nss_edges;
    return true;
}
#define uni_hal_gpio_pin_set set_nss
#endif
#define uni_hal_systick_get_ms frozen_tick
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include "spi/uni_hal_spi_stm32h7.c"
#else
#include "spi/uni_hal_spi_stm32l4.c"
#endif

static unsigned check(void) {
    SystemCoreClock = 16000U;
    uni_hal_spi_context_t ctx = {
        .config = {.instance = UNI_HAL_CORE_PERIPH_SPI_1, .mode = UNI_HAL_SPI_MODE_MASTER},
        .status.inited = true,
    };
    uint8_t const byte = 0x59;
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
    g_uni_hal_spi_ctx[0] = &ctx;
#endif
    for (unsigned final_frame = 0; final_frame <= 1; ++final_frame) {
        // First stall before TX can accept a byte, then after the registers have accepted it.
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
        spi.SR = final_frame ? SPI_SR_TXP | SPI_SR_RXP : 0U;
#else
        spi.SR = final_frame ? SPI_SR_TXE | SPI_SR_RXNE | SPI_SR_BSY : 0U;
#endif
        polls = 0;
        __disable_irq();
        bool const completed = uni_hal_spi_transmit(&ctx, &byte, 1U);
        CHECK(__get_PRIMASK() != 0U);
        __enable_irq();
        CHECK(!completed && polls < 16000U);
        CHECK(!LL_SPI_IsEnabled(&spi) && !ctx.status.in_process);
#if defined(UNI_HAL_TARGET_MCU_STM32H743)
        CHECK(nss_high && nss_edges == 2U + 4U * final_frame);
#endif
        // Already-ready hardware remains usable with the same stopped SysTick.
        spi.SR = ready_status();
        __disable_irq();
        bool const retried = uni_hal_spi_transmit(&ctx, &byte, 1U);
        CHECK(__get_PRIMASK() != 0U);
        __enable_irq();
        CHECK(retried);
    }
    return 0;
}

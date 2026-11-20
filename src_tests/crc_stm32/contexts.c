#if defined(UNI_HAL_TARGET_MCU_STM32H743)
#include <stm32h7xx_ll_crc.h>
#else
#include <stm32l4xx_ll_crc.h>
#endif
#include <stdbool.h>
#include "../arm/semihost_test.h"
#include "core/uni_hal_core.h"

static CRC_TypeDef regs;
#undef CRC
#define CRC (&regs)

static void (*on_resume)(void);
static bool wrote_unmasked;
static unsigned bytes_fed, bytes_before_interrupt;

static void resume(uint32_t mask) {
    uni_hal_core_irq_resume(mask);
    if (on_resume && __get_PRIMASK() == 0U) {
        void (*callback)(void) = on_resume;
        on_resume = nullptr;
        callback();
    }
}

// Configure the real ST registers in RAM. Emulate their RESET and byte/word data writes.
static unsigned width(void) {
    switch (regs.CR & CRC_CR_POLYSIZE) {
    case LL_CRC_POLYLENGTH_7B: return 7U;
    case LL_CRC_POLYLENGTH_8B: return 8U;
    case LL_CRC_POLYLENGTH_16B: return 16U;
    default: return 32U;
    }
}

static void reset_unit(CRC_TypeDef *bank) {
    bank->DR = bank->INIT & (UINT32_MAX >> (32U - width()));
}

static void feed8(CRC_TypeDef *bank, uint8_t byte) {
    ++bytes_fed;
    wrote_unmasked |= __get_PRIMASK() == 0U;
    unsigned bits = width();
    uint32_t remainder = bank->DR;
    for (unsigned i = 0; i < 8U; ++i) {
        bool feedback = ((remainder >> (bits - 1U)) ^ (byte >> (7U - i))) & 1U;
        remainder = (remainder << 1U) ^ (feedback ? bank->POL : 0U);
    }
    bank->DR = remainder & (UINT32_MAX >> (32U - bits));
}

static void feed32(CRC_TypeDef *bank, uint32_t word) {
    for (unsigned i = 0; i < 4U; ++i) {
        feed8(bank, (uint8_t)(word >> (24U - 8U * i)));
    }
}

#define LL_CRC_ResetCRCCalculationUnit reset_unit
#define LL_CRC_FeedData8 feed8
#define LL_CRC_FeedData32 feed32
#define uni_hal_rcc_clk_set test_gate
#define uni_hal_core_irq_resume resume
#include "crc/uni_hal_crc_stm32.c"

bool test_gate(uni_hal_core_periph_e instance, bool enable) {
    return instance == UNI_HAL_CORE_PERIPH_CRC_1 && enable;
}

static uni_hal_crc_context_t *peer;
static uint32_t peer_result;

static void interrupt_calculation(void) {
    static const uint8_t message[] = "123456789";
    bytes_before_interrupt = bytes_fed;
    peer_result = uni_hal_crc_calculate(peer, message, 9U);
}

static __attribute__((noinline)) unsigned check_contexts(void) {
    uni_hal_crc_context_t a = {.instance = UNI_HAL_CORE_PERIPH_CRC_1,
        .polynomial_size = UNI_HAL_CRC_POLYSIZE_8B, .polynomial_coef = 0x07U};
    uni_hal_crc_context_t b = a;
    b.polynomial_coef = 0x31U;
    static const uint8_t message[] = "123456789";
    CHECK(uni_hal_crc_init(&a) && uni_hal_crc_init(&b));
    CHECK(uni_hal_crc_calculate(&a, message, 9U) == 0xF4U);
    CHECK(uni_hal_crc_calculate(&b, message, 9U) == 0xA2U);

    // Interleave two streams with different widths and initial values.
    b.polynomial_size = UNI_HAL_CRC_POLYSIZE_16B;
    b.polynomial_coef = 0x1021U;
    b.initial_data = 0xFFFFU;
    CHECK(uni_hal_crc_init(&b) && uni_hal_crc_reset(&a));
    CHECK(uni_hal_crc_append(&a, message, 4U));
    uint32_t saved = uni_hal_crc_get_value(&a);
    CHECK(uni_hal_crc_get_value(&b) == 0xFFFFU);
    CHECK(uni_hal_crc_append(&b, message, 3U));
    CHECK(uni_hal_crc_get_value(&a) == saved);
    CHECK(uni_hal_crc_append(&a, message + 4U, 5U));
    CHECK(uni_hal_crc_append(&b, message + 3U, 6U));
    CHECK(uni_hal_crc_get_value(&a) == 0xF4U && uni_hal_crc_get_value(&b) == 0x29B1U);
    CHECK(uni_hal_crc_calculate_ex(&b, 0U, message, 9U) == 0x31C3U);

    uint8_t data[257];
    for (unsigned i = 0; i < sizeof(data); ++i) {
        data[i] = (uint8_t)i;
    }
    bytes_fed = 0U;
    peer = &a;
    on_resume = interrupt_calculation;
    CHECK(uni_hal_crc_calculate(&b, data, sizeof(data)) == 0x7abcU);
    CHECK(bytes_before_interrupt > 0U && bytes_before_interrupt < sizeof(data));
    CHECK(peer_result == 0xF4U && !wrote_unmasked);

    b.polynomial_size = UNI_HAL_CRC_POLYSIZE_32B;
    b.polynomial_coef = 0x04C11DB7U;
    b.initial_data = UINT32_MAX;
    CHECK(uni_hal_crc_init(&b));
    CHECK(uni_hal_crc_calculate(&b, message, 9U) == 0x0376E6E7U);
    a.polynomial_size = UNI_HAL_CRC_POLYSIZE_7B;
    a.polynomial_coef = 0x09U;
    CHECK(uni_hal_crc_init(&a));
    CHECK(uni_hal_crc_calculate(&a, message, 9U) == 0x75U);
    CHECK(uni_hal_crc_reset_ex(&a, UINT32_MAX) && uni_hal_crc_get_value(&a) == 0x7FU);
    CHECK(uni_hal_crc_get_value(&b) == 0x0376E6E7U);
    return 0U;
}

static unsigned check(void) {
    // The minimal test vector bypasses SystemInit. GCC may use FP registers for copies.
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    __DSB();
    __ISB();
    return check_contexts();
}

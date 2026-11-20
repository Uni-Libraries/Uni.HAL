#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

#include <stdint.h>


//
// Structs
//

typedef struct {
    /**
     * PLL pre-divider
     */
    uint32_t m;

    /**
     * PLL multiplier
     */
    uint32_t n;

    /**
     * PLL post-divider (output P), zero disables this output
     */
    uint32_t p;

    /**
     * PLL post-divider (output Q), zero disables this output
     */
    uint32_t q;

    /**
     * PLL post-divider (output R), zero disables this output
     */
    uint32_t r;

    /**
     * Fractional numerator (0..8192) in units of 1/8192.
     * At 8192 the multiplier becomes N + 1, which must fit the hardware range 4..512.
     */
    uint32_t fracn;
} uni_hal_rcc_stm32h7_config_pll_t;


typedef struct {
    /**
     * Timeout in msecs before CSI will be marked as failed
     */
    uint32_t csi;

    /**
     * Timeout in msecs before LSE will be marked as failed
     */
    uint32_t lse;

    /**
     * Timeout in msecs before LSI will be marked as failed
     */
    uint32_t lsi;

    /**
     * Timeout in msecs before HSE will be marked as failed
     */
    uint32_t hse;

    /**
     * Timeout in msecs before HSI will be marked as failed
     */
    uint32_t hsi;

    /**
     * Timeout in msecs before PLL will be marked as failed
     */
    uint32_t pll;
} uni_hal_rcc_stm32h7_config_timeout_t;


/**
 * STM RCC interface config context
 */
typedef struct {
    bool                                 hse_enable;
    bool                                 hse_bypass;
    bool                                 hse_css;
    bool                                 csi_enable;
    bool                                 lse_enable;

    /**
     * Reset the backup domain and retry once when LSE does not start.
     * This erases the RTC and the backup registers, so it is off unless asked for.
     */
    bool                                 lse_backup_reset;

    uni_hal_rcc_stm32h7_config_pll_t      pll[3];
    uni_hal_rcc_stm32h7_config_timeout_t  timeout;
} uni_hal_rcc_stm32h7_config_t;



/**
 * STM RCC interface status context
 */
typedef struct {
    /** True when the configured PLLs and nominal system clock are available. */
    bool inited;

    /**
     * CSI initialization was successful
     */
    bool csi_inited;

    /**
     * Backup domain reset was performed during LSE initialization
     */
    bool lse_backup_reseted;

    /**
     * LSE initialization was successful
     */
    bool lse_inited;

    /**
     * LSI initialization was successful
     */
    bool lsi_inited;

    /**
     * HSE initialization was successful
     */
    bool hse_inited;

    /**
     * HSI initialization was successful
     */
    bool hsi_inited;

    /**
     * PLL initialization was successful
     */
    bool pll_inited[3];

    /**
     * SYSCLK has been confirmed. After CSS this can also indicate direct HSI fallback.
     */
    bool sys_inited;

    /**
     * HSE clock security system fault happened
     */
    bool css_fault;

    /** During startup, the interrupted init completes recovery after the NMI returns. */
    bool css_recovery_pending;

} uni_hal_rcc_stm32h7_status_t;



//
// Functions
//

/**
 * Copy the configuration for startup and clock recovery. Returns false during clock
 * initialization or when an enabled PLL's fractional part cannot be represented.
 */
bool uni_hal_rcc_stm32h7_config_set(const uni_hal_rcc_stm32h7_config_t *config);

uni_hal_rcc_stm32h7_status_t uni_hal_rcc_stm32h7_status_get(void);

/**
 * Get the reason for the last reset
 * @return RCC_RSR as it was when uni_hal_rcc_init() ran: test it with the RCC_RSR_xxxRSTF
 *         masks, e.g. RCC_RSR_IWDG1RSTF for an independent watchdog reset. 0 before the init.
 */
uint32_t uni_hal_rcc_stm32h7_reset_flags_get(void);

uint32_t uni_hal_rcc_stm32h7_clk_get_hclk(void);

#if defined(__cplusplus)
}
#endif

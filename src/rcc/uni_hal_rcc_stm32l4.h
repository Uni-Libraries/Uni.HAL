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
     * PLL post-divider (output P)
     */
    uint32_t p;

    /**
     * PLL post-divider (output Q)
     */
    uint32_t q;

    /**
     * PLL post-divider (output R)
     */
    uint32_t r;
} uni_hal_rcc_stm32l4_config_pll_t;


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
} uni_hal_rcc_stm32l4_config_timeout_t;


/**
 * Drive level of the LSE oscillator. A crystal with a high load capacitance or a high series
 * resistance needs a higher level to start; a higher level draws more current.
 */
typedef enum {
    UNI_HAL_RCC_STM32L4_LSE_DRIVE_LOW = 0,
    UNI_HAL_RCC_STM32L4_LSE_DRIVE_MEDIUM_LOW,
    UNI_HAL_RCC_STM32L4_LSE_DRIVE_MEDIUM_HIGH,
    UNI_HAL_RCC_STM32L4_LSE_DRIVE_HIGH,
} uni_hal_rcc_stm32l4_lse_drive_e;


/**
 * STM RCC interface config context
 */
typedef struct {
    bool                                 hse_enable;
    bool                                 hse_bypass;
    bool                                 hse_css;
    bool                                 lse_enable;

    /**
     * Reset the backup domain and retry once when LSE does not start.
     * This erases the RTC and the backup registers, so it is off unless asked for.
     */
    bool                                 lse_backup_reset;

    /**
     * Drive level LSE is started with. An LSE that is already running, kept alive by VBAT
     * across a reset, is left as it is: the level cannot be raised while the oscillator is on.
     */
    uni_hal_rcc_stm32l4_lse_drive_e      lse_drive;
    uni_hal_rcc_stm32l4_config_pll_t      pll[1]; //TODO: add support for PLL2 and PLL3
    uni_hal_rcc_stm32l4_config_timeout_t  timeout;
} uni_hal_rcc_stm32l4_config_t;



/**
 * STM RCC interface status context
 */
typedef struct {
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
    bool pll_inited;

    /**
     * SYS clock initialization was successful
     */
    bool sys_inited;
} uni_hal_rcc_stm32l4_status_t;



//
// Functions
//

bool uni_hal_rcc_stm32l4_config_set(uni_hal_rcc_stm32l4_config_t* config);

/**
 * Get the state of the oscillators and of the clock tree
 * @return what came up in uni_hal_rcc_init(). The clock security system keeps it current: after
 *         an HSE failure hse_inited is false, and pll_inited and sys_inited tell whether the
 *         switch to HSI worked.
 */
uni_hal_rcc_stm32l4_status_t uni_hal_rcc_stm32l4_status_get(void);

#if defined(__cplusplus)
}
#endif

#pragma once

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
 * STM RCC interface config context
 */
typedef struct {
    bool                                 hse_enable;
    bool                                 hse_bypass;
    bool                                 hse_css;

    bool                                 csi_enable;

    bool                                 lse_enable;

    uni_hal_rcc_stm32l4_config_pll_t      pll[3];
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

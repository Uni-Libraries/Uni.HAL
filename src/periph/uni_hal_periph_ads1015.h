#pragma once

//
// Includes
//

// stdlib
#include <stdint.h>

// uni_hal
#include "i2c/uni_hal_i2c.h"



//
// Enums
//

typedef enum
{
    UNI_HAL_ADS1015_MODE_CONT = 0x0000,
    UNI_HAL_ADS1015_MODE_SINGLE = 0x0100,
} uni_hal_ads1015_mode_e;


typedef enum
{
    UNI_HAL_ADS1015_MUX_IN0_IN1 = 0x0000,
    UNI_HAL_ADS1015_MUX_IN0_IN3 = 0x1000,
    UNI_HAL_ADS1015_MUX_IN1_IN3 = 0x2000,
    UNI_HAL_ADS1015_MUX_IN2_IN3 = 0x3000,
    UNI_HAL_ADS1015_MUX_IN0_GND = 0x4000,
    UNI_HAL_ADS1015_MUX_IN1_GND = 0x5000,
    UNI_HAL_ADS1015_MUX_IN2_GND = 0x6000,
    UNI_HAL_ADS1015_MUX_IN3_GND = 0x7000,
} uni_hal_ads1015_mux_e;

typedef enum
{
    UNI_HAL_ADS1015_CONFIG_PGA_6144 = 0X0000,  // +/- 6.144v
    UNI_HAL_ADS1015_CONFIG_PGA_4096 = 0X0200,  // +/- 4.096v
    UNI_HAL_ADS1015_CONFIG_PGA_2048 = 0X0400,  // +/- 2.048v
    UNI_HAL_ADS1015_CONFIG_PGA_1024 = 0X0600,  // +/- 1.024v
    UNI_HAL_ADS1015_CONFIG_PGA_0512 = 0X0800,  // +/- 0.512v
    UNI_HAL_ADS1015_CONFIG_PGA_0256 = 0X0A00,  // +/- 0.256v
} uni_hal_ads1015_pga_e;

typedef enum
{
    UNI_HAL_ADS1015_RATE_128HZ  = 0x0000,
    UNI_HAL_ADS1015_RATE_250HZ  = 0x0020,
    UNI_HAL_ADS1015_RATE_490HZ  = 0x0040,
    UNI_HAL_ADS1015_RATE_920HZ  = 0x0060,
    UNI_HAL_ADS1015_RATE_1600HZ = 0x0080,
    UNI_HAL_ADS1015_RATE_2400HZ = 0x00A0,
    UNI_HAL_ADS1015_RATE_3300HZ = 0x00C0,
} uni_hal_ads1015_rate_e;

typedef enum
{
    UNI_HAL_ADS1015_REG_CONVERSION = 0x00,
    UNI_HAL_ADS1015_REG_CONFIG     = 0x01
} uni_hal_ads1015_reg_e;

/*
#define ADS1015_ADR_CONVERS      0x00
#define ADS1015_ADR_CONFIG       0x01
#define ADS1015_ADR_LOWTHRESH    0x02
#define ADS1015_ADR_HITHRESH     0x03
*/

typedef enum
{
    UNI_HAL_ADS1015_STATUS_NOEFF = 0x0000,
} uni_hal_ads1015_status_e;



//
// Structs
//

typedef struct {
    uni_hal_i2c_context_t* i2c;
    uint32_t timeout;
    uint8_t address;

    uni_hal_ads1015_mode_e mode;
    uni_hal_ads1015_mux_e mux;
    uni_hal_ads1015_rate_e rate;
    uni_hal_ads1015_pga_e pga;

} uni_hal_ads1015_config_t;


typedef struct {
    bool initialized;
} uni_hal_ads1015_state_t;


typedef struct {
    uni_hal_ads1015_config_t config;
    uni_hal_ads1015_state_t state;
} uni_hal_ads1015_context_t;



//
// Functions
//

bool uni_hal_ads1015_init(uni_hal_ads1015_context_t* ctx);

bool uni_hal_ads1015_is_inited(const uni_hal_ads1015_context_t* ctx);

bool uni_hal_ads1015_configure(uni_hal_ads1015_context_t* ctx);

uint16_t uni_hal_ads1015_get_raw(uni_hal_ads1015_context_t* ctx);

int16_t uni_hal_ads1015_get_voltage_mv(uni_hal_ads1015_context_t* ctx);
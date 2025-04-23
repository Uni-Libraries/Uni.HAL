#pragma once

//
// Includes
//

// uni_hal
#include "i2c/uni_hal_i2c.h"



//
// Enums
//

typedef enum
{
    UNI_HAL_MCP23017_REG_IODIR_A = 0x00,
    UNI_HAL_MCP23017_REG_IODIR_B = 0x01,
    UNI_HAL_MCP23017_REG_GPIO_A  = 0x12,
    UNI_HAL_MCP23017_REG_GPIO_B  = 0x13,
} uni_hal_mcp23017_reg_e;


typedef enum {
    UNI_HAL_MCP23017_PORT_A,
    UNI_HAL_MCP23017_PORT_B,
} uni_hal_mcp23017_port_e;



//
// Structs
//

typedef struct {
    uni_hal_i2c_context_t* i2c;
    uint32_t timeout;
    uint8_t address;
} uni_hal_mcp23017_config_t;


typedef struct {
    bool initialized;
} uni_hal_mcp23017_state_t;


typedef struct {
    uni_hal_mcp23017_config_t config;
    uni_hal_mcp23017_state_t state;
} uni_hal_mcp23017_context_t;



//
// Functions
//

bool uni_hal_mcp23017_init(uni_hal_mcp23017_context_t* ctx);

bool uni_hal_mcp23017_is_inited(const uni_hal_mcp23017_context_t* ctx);

bool uni_hal_mcp23017_get_gpio(uni_hal_mcp23017_context_t* ctx, uni_hal_mcp23017_port_e port, uint8_t *val);

bool uni_hal_mcp23017_set_iodir(uni_hal_mcp23017_context_t* ctx, uni_hal_mcp23017_port_e port, uint8_t direction);

bool uni_hal_mcp23017_set_gpio(uni_hal_mcp23017_context_t* ctx, uni_hal_mcp23017_port_e port, uint8_t val);

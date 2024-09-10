#pragma once

//
// Includes
//

// uni_hal
#include "i2c/uni_hal_i2c.h"



//
// Typedefs
//

typedef struct {
    uni_hal_i2c_context_t* i2c;
    uint32_t timeout;
    uint8_t address;
} uni_hal_c15b064j_config_t;


typedef struct {
    bool initialized;
} uni_hal_c15b064j_state_t;


typedef struct {
    uni_hal_c15b064j_config_t config;
    uni_hal_c15b064j_state_t state;
} uni_hal_c15b064j_context_t;



//
// Functions
//

bool uni_hal_c15b064j_init(uni_hal_c15b064j_context_t* ctx);

bool uni_hal_c15b064j_is_inited(const uni_hal_c15b064j_context_t* ctx);

bool uni_hal_c15b064j_read(uni_hal_c15b064j_context_t* ctx, uint16_t address, uint16_t len, uint8_t* buf);

bool uni_hal_c15b064j_write(uni_hal_c15b064j_context_t* ctx, uint16_t address, uint16_t len, uint8_t* buf);

//
// Includes
//

// stdlib
#include <stddef.h>

// uni_hal
#include "i2c/uni_hal_i2c.h"



//
// Functions
//

bool uni_hal_i2c_init(uni_hal_i2c_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        ctx->state.initialized = true;
        result = true;
    }
    return result;
}


bool uni_hal_i2c_deinit(uni_hal_i2c_context_t *ctx) {
    bool result = false;
    if (ctx != NULL) {
        ctx->state.initialized = false;
        result = true;
    }
    return result;
}


bool uni_hal_i2c_isready(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint32_t trials, uint32_t timeout){
    bool result = false;
    if(uni_hal_i2c_is_inited(ctx)){
        (void)dev_addr;
        (void)trials;
        (void)timeout;
        result =true;
    }
    return result;
}


bool uni_hal_i2c_master_receive(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout){
    bool result = false;
    if(uni_hal_i2c_is_inited(ctx)){
        (void)dev_addr;
        (void)buf;
        (void)buf_len;
        (void)timeout;
        result =true;
    }
    return result;
}


bool uni_hal_i2c_master_transmit(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint8_t *buf, uint16_t buf_len, uint32_t timeout){
    bool result = false;
    if(uni_hal_i2c_is_inited(ctx)){
        (void)dev_addr;
        (void)buf;
        (void)buf_len;
        (void)timeout;
        result =true;
    }
    return result;
}



bool uni_hal_i2c_mem_read(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                         uint8_t *buf, uint16_t buf_len, uint32_t timeout){
    bool result = false;
    if(uni_hal_i2c_is_inited(ctx)){
        (void)dev_addr;
        (void)mem_addr;
        (void)mem_addr_size;
        (void)buf;
        (void)buf_len;
        (void)timeout;
        result =true;
    }
    return result;
}



bool uni_hal_i2c_mem_write(uni_hal_i2c_context_t *ctx, uint16_t dev_addr, uint16_t mem_addr, uni_hal_i2c_memadd_size_e mem_addr_size,
                          uint8_t *buf, uint16_t buf_len, uint32_t timeout){
    bool result = false;
    if(uni_hal_i2c_is_inited(ctx)){
        (void)dev_addr;
        (void)mem_addr;
        (void)mem_addr_size;
        (void)buf;
        (void)buf_len;
        (void)timeout;
        result =true;
    }
    return result;
}

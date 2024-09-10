//
// Includes
//

#include <stddef.h>

#include "rcc/uni_hal_rcc.h"



//
// Functions
//

bool uni_hal_rcc_init() {
    return true;
}


bool uni_hal_rcc_is_inited(){
    return true;
}


uint8_t uni_hal_rcc_get_status_reg(){
    return 1;
}


bool uni_hal_rcc_get_status_reg_flag(uint8_t flag) {
    (void) flag;
    return true;
}


bool uni_hal_rcc_reset(uni_hal_rcc_reset_target_e target) {
    (void) target;
    return true;
}


bool uni_hal_rcc_clk_get(uni_hal_core_periph_e target) {
    (void) target;
    return true;
}


uint32_t uni_hal_rcc_clk_get_freq(uni_hal_core_periph_e target){
    (void) target;
    return 1U;
}


bool uni_hal_rcc_clk_set(uni_hal_core_periph_e target, bool state){
    (void) target;
    (void) state;
    return true;
}


uni_hal_rcc_clksrc_e uni_hal_rcc_clksrc_get(uni_hal_core_periph_e target){
    (void) target;
    return UNI_HAL_RCC_CLKSRC_NONE;
}


bool uni_hal_rcc_clksrc_set(uni_hal_core_periph_e target, uni_hal_rcc_clksrc_e source){
    (void) target;
    (void) source;
    return true;
}

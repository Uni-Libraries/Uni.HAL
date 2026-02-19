//
// Includes
//

#include "dwt/uni_hal_dwt.h"
#include "dwt/uni_hal_dwt_internal.h"



//
// Functions
//


bool uni_hal_dwt_init(void){
    return true;
}


bool uni_hal_dwt_is_inited(void){
    return true;
}


void uni_hal_dwt_delay_us(uint32_t us){
    (void)us;
}


uint32_t uni_hal_dwt_get_tick(void){
    return 0U;
}


uint32_t uni_hal_dwt_convert_ms(uint32_t tick) {
    (void)tick;
    return 0U;
}


uint32_t uni_hal_dwt_convert_ns(uint32_t tick) {
    (void)tick;
    return 0U;
}


uint32_t uni_hal_dwt_convert_us(uint32_t tick) {
    (void)tick;
    return 0U;
}


uint32_t uni_hal_dwt_timeout_ms_to_tick(uint32_t timeout_ms) {
    // PC implementation maps milliseconds to pseudo-ticks 1:1.
    return uni_hal_dwt_internal_timeout_ms_to_tick(timeout_ms, UNI_HAL_DWT_UNITS_PER_SECOND_MS);
}

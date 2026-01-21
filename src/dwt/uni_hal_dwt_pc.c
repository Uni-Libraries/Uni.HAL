//
// Includes
//

#include "dwt/uni_hal_dwt.h"

#include <limits.h>



//
// Functions
//


bool uni_hal_dwt_init(void){
    return true;
}


bool uni_hal_dwt_is_inited(void){
    return true;
}


void uni_hal_dwt_delay_ms(uint32_t ms){
    (void)ms;
}


void uni_hal_dwt_delay_us(uint32_t us){
    (void)us;
}


uint32_t uni_hal_dwt_get_ms(void){
    return 0U;
}


uint32_t uni_hal_dwt_get_us(void){
    return 0U;
}


uint32_t uni_hal_dwt_get_tick(void){
    return 0U;
}


uint32_t uni_hal_dwt_compare(uint32_t timestamp_1, uint32_t timestamp_2) {
    return (uint32_t)(timestamp_2 - timestamp_1);
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

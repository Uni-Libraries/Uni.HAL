//
// Includes
//

#include "dwt/uni_hal_dwt.h"
#include "dwt/uni_hal_dwt_internal.h"


//
// Functions
//

uint32_t uni_hal_dwt_get_ms(void)
{
    return uni_hal_dwt_convert_ms(uni_hal_dwt_get_tick());
}


uint32_t uni_hal_dwt_get_us(void)
{
    return uni_hal_dwt_convert_us(uni_hal_dwt_get_tick());
}


uint32_t uni_hal_dwt_compare(uint32_t timestamp_1, uint32_t timestamp_2)
{
    // Unsigned subtraction is modulo 2^32 and therefore naturally handles overflow.
    return (uint32_t)(timestamp_2 - timestamp_1);
}


void uni_hal_dwt_delay_ms(uint32_t ms)
{
    const uint32_t max_ms = UINT32_MAX / UNI_HAL_DWT_UNITS_PER_MILLISECOND;
    while (ms > max_ms) {
        uni_hal_dwt_delay_us(max_ms * UNI_HAL_DWT_UNITS_PER_MILLISECOND);
        ms -= max_ms;
    }
    uni_hal_dwt_delay_us(ms * UNI_HAL_DWT_UNITS_PER_MILLISECOND);
}

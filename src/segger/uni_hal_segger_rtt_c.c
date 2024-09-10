//
// Includes
//

// uni_hal
#include "uni_hal_segger_rtt.h"

#if !defined(UNI_HAL_TARGET_MCU_PC)

// segger
#include "SEGGER_RTT.h"

#endif



//
// Private
//

static bool g_uni_hal_segger_rtt_inited = false;



//
// Implementation
//


void uni_hal_segger_rtt_init(void) {
#if !defined(UNI_HAL_TARGET_MCU_PC)
    if (!g_uni_hal_segger_rtt_inited) {
        SEGGER_RTT_Init();
        g_uni_hal_segger_rtt_inited = true;
    }
#endif
}


bool uni_hal_segger_rtt_is_inited(void) {
    return g_uni_hal_segger_rtt_inited;
}

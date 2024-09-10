//
// Includes
//

// stdlib
#include <stdarg.h>
#include <stdio.h>

// segger
#if !defined(UNI_HAL_TARGET_MCU_PC)
    #include "SEGGER_RTT.h"
#endif

void vLoggingPrintf( const char * pcFormat, ... ) {
    va_list ParamList;
    va_start(ParamList, sFormat);
#if defined(UNI_HAL_TARGET_MCU_PC)
    vprintf(pcFormat, ParamList);
#else
    SEGGER_RTT_vprintf(0, pcFormat, &ParamList);
#endif
    va_end(ParamList);
}

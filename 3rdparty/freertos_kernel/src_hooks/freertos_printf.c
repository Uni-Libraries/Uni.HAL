//
// Includes
//

// stdlib
#include <stdarg.h>
#include <stdio.h>



//
// Functions
//

void vLoggingPrintf( const char * pcFormat, ... ) {
    va_list ParamList;
    va_start(ParamList, sFormat);
    vprintf(pcFormat, ParamList);
    va_end(ParamList);
}

//
// Includes
//

// stdlib
#include <stdarg.h>
#include <stdio.h>



//
// Functions
//

#if defined(_MSC_VER)
    int uni_hal_io_stdio_vprintf(const char* format, va_list);
#else
    int uni_hal_io_stdio_vprintf(const char* format, va_list) __attribute__((format(printf, 1, 0)));
#endif

void vLoggingPrintf( const char * pcFormat, ... ) {
    va_list ParamList;
    va_start(ParamList, pcFormat);
    uni_hal_io_stdio_vprintf(pcFormat, ParamList);
    va_end(ParamList);
}

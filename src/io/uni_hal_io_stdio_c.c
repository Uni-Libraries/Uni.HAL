//
// Includes
//

// uni_hal
#include "nanoprintf.h"
#include "uni_common.h"
#include "uni_hal_io_stdio.h"



//
// Globals
//

uni_hal_stdio_context_t g_uni_hal_io_stdio_ctx = { .io_context = NULL };



//
// Private
//

static void _uni_hal_io_stdio_putc_npf(int c, void* ptr) {
    (void)ptr;
    uint8_t cc = c;
    uni_hal_io_transmit_data(g_uni_hal_io_stdio_ctx.io_context, &cc, 1U);
}



//
// Public functions
//

bool uni_hal_io_stdio_init(uni_hal_io_context_t *io_ctx) {
    g_uni_hal_io_stdio_ctx.io_context = io_ctx;
    return true;
}

int uni_hal_io_stdio_printf(const char *format, ...) {
    va_list val;
    va_start(val, format);
    int const rv = npf_vpprintf(&_uni_hal_io_stdio_putc_npf, NULL, format, val);
    va_end(val);
    return rv;
}

int uni_hal_io_stdio_vprintf(const char * format, va_list params) {
    return npf_vpprintf(&_uni_hal_io_stdio_putc_npf, NULL, format, params);
}

int uni_hal_io_stdio_snprintf(char *buffer, size_t buffer_size, const char* format, ...) {
    va_list val;
    va_start(val, format);
    int const rv = npf_vsnprintf(buffer, buffer_size, format, val);
    va_end(val);
    return rv;
}

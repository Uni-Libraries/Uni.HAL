#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//
// Includes
//

// stdlib
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

// uni_hal
#include "uni_hal_io.h"


//
// Typedefs
//

/**
 * STDIO I/O context
 */
typedef struct {
    /**
     * IO source type;
     */
    uni_hal_io_context_t* io_context;

} uni_hal_stdio_context_t;



//
// Functions
//

/**
 * Initializes standard IO
 * @param io_ctx pointer to the I/O context
 * @return bool on success
 */
bool uni_hal_io_stdio_init(uni_hal_io_context_t *io_ctx);

#if defined(_MSC_VER)
int uni_hal_io_stdio_printf(const char* format, ...);
int uni_hal_io_stdio_vprintf(const char* format, va_list params);
int uni_hal_io_stdio_snprintf(char *buffer, size_t buffer_size, const char* format, ...);
#else
int uni_hal_io_stdio_printf(const char* format, ...) __attribute__((format(printf, 1, 2)));
int uni_hal_io_stdio_vprintf(const char* format, va_list params) __attribute__((format(printf, 1, 0)));
int uni_hal_io_stdio_snprintf(char *buffer, size_t buffer_size, const char* format, ...) __attribute__((format(printf, 3, 4)));
#endif

#if defined(__cplusplus)
}
#endif

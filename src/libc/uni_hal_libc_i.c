//
// Includes
//

// stdlib
#include <errno.h>
#include <unistd.h>


#if defined(NEWLIB)

#if !defined(__clang__)
#include <reent.h>
#endif

// uni_hal
#include "uni_hal_common_libc.h"



//
// Implementation
//

int *__errno(void) {
#if defined(__clang__)
    return NULL;
#else
    return &_REENT->_errno;
#endif
}

int _close(int) {
    errno = ENOSYS;
    return -1;
}

int _kill(int, int) {
    errno = ENOSYS;
    return -1;
}

off_t _lseek(int, off_t, int) {
    errno = ENOSYS;
    return -1;
}

int _getpid() {
    errno = ENOSYS;
    return -1;
}

//
// Includes
//

// stdlib
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>

// uni_hal
#include "uni_hal_common.h"
#include "uni_hal_io.h"
#include "uni_hal_io_stdio.h"



//
// Globals
//

extern uni_hal_stdio_context_t g_uni_hal_io_stdio_ctx;

#if defined(__clang__)
FILE* const stdout = nullptr;
#endif


//
// Standard I/O Functions
//

int _read(int UNI_HAL_COMPILER_UNUSED_VAR(file), char *ptr, int len) {
    int result = -1;

    if (g_uni_hal_io_stdio_ctx.io_context && ptr) {
        result = (int)uni_hal_io_receive_data(g_uni_hal_io_stdio_ctx.io_context, (uint8_t *)ptr, len, 0);
    }

    if (result < 0) {
        errno = EINVAL;
    }

    return result;
}


ssize_t _write(int UNI_HAL_COMPILER_UNUSED_VAR(file), const void *ptr, size_t len) {
    int result = -1;

    if (g_uni_hal_io_stdio_ctx.io_context && ptr) {
        result = (int) uni_hal_io_transmit_data(g_uni_hal_io_stdio_ctx.io_context, (const uint8_t *) ptr, len);
    }

    if (result < 0) {
        errno = EINVAL;
    }

    return result;
}


int _fstat(int UNI_HAL_COMPILER_UNUSED_VAR(file), struct stat *st) {
    if (st != nullptr) {
        st->st_mode = S_IFCHR;
    }
    return 0;
}


int _isatty(int UNI_HAL_COMPILER_UNUSED_VAR(file)) { return 1; }


#endif


//
// Includes
//

// stdlib
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include "io/uni_hal_io.h"
#include "io/uni_hal_io_stdio.h"

// newlib
#if defined(_NEWLIB_VERSION) && !defined(__clang__)
#include <reent.h>
#endif



//
// Globals
//

extern uni_hal_stdio_context_t g_uni_hal_io_stdio_ctx;

#if defined(_NEWLIB_VERSION) && defined(__clang__)
FILE* const stdout = nullptr;
#endif



//
// Functions
//

#if !defined(UNI_HAL_TARGET_MCU_PC)

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


int _fstat(int UNI_COMMON_COMPILER_UNUSED_VAR(file), struct stat *st) {
    if (st != nullptr) {
        st->st_mode = S_IFCHR;
    }
    return 0;
}


int _isatty(int UNI_COMMON_COMPILER_UNUSED_VAR(file)) { return 1; }


int _read(int UNI_COMMON_COMPILER_UNUSED_VAR(file), char *ptr, int len) {
    int result = -1;

    if (g_uni_hal_io_stdio_ctx.io_context && ptr) {
        result = (int)uni_hal_io_receive_data(g_uni_hal_io_stdio_ctx.io_context, (uint8_t *)ptr, len, 0);
    }

    if (result < 0) {
        errno = EINVAL;
    }

    return result;
}


ssize_t _write(int UNI_COMMON_COMPILER_UNUSED_VAR(file), const void *ptr, size_t len) {
    int result = -1;

    if (g_uni_hal_io_stdio_ctx.io_context && ptr) {
        result = (int) uni_hal_io_transmit_data(g_uni_hal_io_stdio_ctx.io_context, (const uint8_t *) ptr, len);
    }

    if (result < 0) {
        errno = EINVAL;
    }

    return result;
}

#endif

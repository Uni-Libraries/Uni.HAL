# Instructions for AI Agents

* Programming language:
    * Use C23 language standard.
    * prefer `nullptr` to `NULL`, do not add fallback for old compilers
    * prefer `false` and `true`, you can add `#include <stdbool.h>`

* Try to not use standard library functions with implicit heap allocation (like printf)
    * Use the following functions from `src/io/uni_hal_io_stdio.h`
        * `printf` -> `uni_hal_io_stdio_printf`
        * `vprintf` -> `uni_hal_io_stdio_vprintf`
        * `snprintf` -> `uni_hal_io_stdio_snprintf`

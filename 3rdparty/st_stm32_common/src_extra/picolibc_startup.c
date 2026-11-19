// The vendor Reset_Handler has initialized .data and .bss at this point.
#include <picotls.h>

extern char __tls_base[];
void __real___libc_init_array(void);

void __wrap___libc_init_array(void) {
    _init_tls(__tls_base);
    _set_tls(__tls_base);
    __real___libc_init_array();
}

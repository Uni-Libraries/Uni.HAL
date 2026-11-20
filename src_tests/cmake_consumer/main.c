#include "systick/uni_hal_systick.h"

int main(void) {
    return uni_hal_systick_init() ? 0 : 1;
}

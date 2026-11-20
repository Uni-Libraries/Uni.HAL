#undef NDEBUG
#include <assert.h>
#include <xparameters_ps.h>
#include <xiltimer.h>
#include "systick/uni_hal_systick_zynq7000.c"

static XTime counter;

void XTime_GetTime(XTime *time) {
    *time = counter;
}

static void check_time(XTime counts, uint32_t expected_ms) {
    counter = counts;
    assert(uni_hal_systick_get_ms() == expected_ms);
    assert(uni_hal_systick_get_tick() == expected_ms);
}

int main(void) {
    const XTime hz = (COUNTS_PER_SECOND);
    assert(hz == 333333333U && uni_hal_systick_get_freq() == 1000U);
    check_time(0U, 0U);
    check_time(333333U, 0U);
    check_time(333334U, 1U);
    check_time(hz - 1U, 999U);
    check_time(1000U * hz - 1U, 999999U);
    const XTime wrap = (((UINT64_C(1) << 32) * hz) + 999U) / 1000U;
    check_time(wrap - 1U, UINT32_MAX);
    check_time(wrap, 0U);
    // floor((2^64 - 1) * 1000 / 333333333) modulo 2^32.
    check_time(UINT64_MAX, UINT32_C(3873634804));
    return 0;
}

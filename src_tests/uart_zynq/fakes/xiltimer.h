#pragma once
#include <stdint.h>
typedef uint64_t XTime;
void XTime_GetTime(XTime *time);
#define COUNTS_PER_SECOND (XPAR_CPU_CORE_CLOCK_FREQ_HZ / 2U)

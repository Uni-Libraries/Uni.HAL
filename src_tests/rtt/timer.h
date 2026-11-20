#pragma once
#include "segger/uni_hal_segger_rtt_stdio.h"

typedef enum { NO_FAILURE, INIT_FAILURE, REGISTER_FAILURE, START_FAILURE } timer_failure_t;
extern timer_failure_t fail_at;
extern unsigned init_calls, stop_calls;
extern bool running;
extern uni_hal_tim_callback_fn callback;
extern void *callback_ctx;

void finish(unsigned code);
#define CHECK(condition) do { if (!(condition)) finish(__LINE__); } while (0)

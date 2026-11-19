#pragma once

#undef configTICK_RATE_HZ
#define configTICK_RATE_HZ 128U
#define configUSE_TICKLESS_IDLE 1

#ifdef UNI_HAL_TEST_SYSTICK_SLEEP
#define UNI_HAL_TEST_SYSTICK_CLOCK_CHANGE 1
void uni_hal_test_pre_sleep(void);
#define configPRE_SLEEP_PROCESSING(idle_ticks) uni_hal_test_pre_sleep()
#endif

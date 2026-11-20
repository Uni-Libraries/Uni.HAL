#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>
#include <stm32l4xx_hal.h>

#include "systick/uni_hal_systick.h"
#include "systick/uni_hal_systick_stm32.h"

extern int __real_main(void);
extern int uni_hal_test_io_buffers(void);
extern int uni_hal_test_cpp_io_definition(void);
extern int uni_hal_test_tunnel_irq(void);
extern int uni_hal_test_receive_irq(void);
extern void SysTick_Handler(void);
extern volatile uint32_t g_uni_hal_systick_counter;

static uint32_t boot_ms;

[[noreturn]] static void exit_qemu(int result) {
    // Arm semihosting SYS_EXIT_EXTENDED reports the test's return code to CTest.
    uint32_t block[] = {0x20026U, (uint32_t)result};
    register uint32_t operation __asm__("r0") = 0x20U;
    register void *args __asm__("r1") = block;
    __asm__ volatile ("bkpt 0xab" : "+r"(operation), "+r"(args) :: "memory");
    for (;;) {}
}

static bool clocks_agree(void) {
    // Read one coherent sample even when QEMU schedules an IRQ between instructions.
    taskENTER_CRITICAL();
    TickType_t ticks = xTaskGetTickCount() - (TickType_t)configINITIAL_TICK_COUNT;
    uint32_t expected = boot_ms + (uint32_t)((uint64_t)ticks * 1000U / configTICK_RATE_HZ);
    bool ok = uni_hal_systick_get_ms() == expected && HAL_GetTick() == expected &&
              uni_hal_systick_get_tick() == expected && uni_hal_systick_get_freq() == 1000U;
    taskEXIT_CRITICAL();
    return ok;
}

#ifdef UNI_HAL_TEST_SYSTICK_CLOCK_CHANGE
static volatile bool change_clock_in_sleep;

// Model the HCLK change delivered by L496 CSS after the port programmed its long reload.
void uni_hal_test_pre_sleep(void) {
    if (change_clock_in_sleep) {
        change_clock_in_sleep = false;
        uint32_t const sleep_reload = SysTick->LOAD;
        uint32_t const old_hclk = SystemCoreClock;
        SystemCoreClock = old_hclk / 2U;
        uni_hal_systick_reclock(old_hclk, SystemCoreClock);
        if (SysTick->LOAD != sleep_reload) {
            exit_qemu(24);
        }
    }
}

static void check_tickless_reclock(void) {
    change_clock_in_sleep = true;
    vTaskDelay(16);
    uint32_t const reload = SystemCoreClock / configTICK_RATE_HZ - 1U;
    if (change_clock_in_sleep || SysTick->LOAD != reload || !clocks_agree()) {
        exit_qemu(25);
    }
    vTaskDelay(16);
    if (SysTick->LOAD != reload || !clocks_agree()) {
        exit_qemu(26);
    }

    // A change in task context immediately rescales the periodic timer. Its next
    // sleep must also use the new calibration when the port restores the reload.
    uint32_t const old_hclk = SystemCoreClock;
    SystemCoreClock = old_hclk * 2U;
    uni_hal_systick_reclock(old_hclk, SystemCoreClock);
    vTaskDelay(16);
    if (SysTick->LOAD != SystemCoreClock / configTICK_RATE_HZ - 1U || !clocks_agree()) {
        exit_qemu(27);
    }
}
#endif

static void run_in_task(void *unused) {
    (void)unused;
#ifdef UNI_HAL_TEST_SYSTICK_CLOCK_CHANGE
    check_tickless_reclock();
#endif
    vTaskDelay(3);
    if (!clocks_agree()) {
        exit_qemu(20);
    }
    vTaskDelay(13);
    if (!clocks_agree()) {
        exit_qemu(21);
    }
    vTaskSuspendAll();
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < 10U) {
        __WFI();
    }
    void *block = malloc(16U);
    if (!block) {
        exit_qemu(1);
    }
    free(block);
    (void)xTaskResumeAll();
    if (!clocks_agree()) {
        exit_qemu(22);
    }
    int result = __real_main();
    if (result == 0) {
        result = uni_hal_test_io_buffers();
    }
    if (result == 0) {
        result = uni_hal_test_cpp_io_definition();
    }
    if (result == 0) {
        result = uni_hal_test_tunnel_irq();
    }
    if (result == 0) {
        result = uni_hal_test_receive_irq();
    }
    exit_qemu(result);
}

int __wrap_main(void) {
    // Drive three 1 ms boot interrupts; the first scheduler ticks then cross the uint32_t wrap.
    g_uni_hal_systick_counter = UINT32_MAX - 4U;
    for (unsigned i = 0; i < 3; ++i) {
        SysTick_Handler();
    }
    boot_ms = UINT32_MAX - 1U;
    if (uni_hal_systick_get_ms() != boot_ms || HAL_GetTick() != boot_ms) {
        exit_qemu(23);
    }
    int result = __real_main();
    if (result == 0 && xTaskCreate(run_in_task, "allocator", 512, nullptr, 1, nullptr) == pdPASS) {
        vTaskStartScheduler();
    }
    exit_qemu(result ? result : 1);
}

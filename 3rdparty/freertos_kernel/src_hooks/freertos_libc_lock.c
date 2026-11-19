#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#if defined(__PICOLIBC__)
#include <semphr.h>
#include <sys/lock.h>

// Picolibc shares this recursive lock between malloc and other libc operations
// including flockfile(), which an application can hold across blocking calls.
#if configSUPPORT_STATIC_ALLOCATION
static StaticSemaphore_t libc_mutex_storage;
#endif
static SemaphoreHandle_t libc_mutex;

void __real___retarget_lock_acquire_recursive(_LOCK_T lock);
void __real___retarget_lock_release_recursive(_LOCK_T lock);

void __wrap___retarget_lock_acquire_recursive(_LOCK_T lock) {
    if (lock != &__lock___libc_recursive_mutex) {
        __real___retarget_lock_acquire_recursive(lock);
        return;
    }
    BaseType_t state = xTaskGetSchedulerState();
    if (state != taskSCHEDULER_NOT_STARTED) {
        taskENTER_CRITICAL();
        if (libc_mutex == nullptr) {
#if configSUPPORT_STATIC_ALLOCATION
            libc_mutex = xSemaphoreCreateRecursiveMutexStatic(&libc_mutex_storage);
#else
            libc_mutex = xSemaphoreCreateRecursiveMutex();
            // This hook cannot return an error or allow unprotected allocation.
            if (libc_mutex == nullptr) {
                __builtin_trap();
            }
#endif
        }
        taskEXIT_CRITICAL();
        // FreeRTOS permits only nonblocking takes while the scheduler is suspended.
        TickType_t wait = state == taskSCHEDULER_SUSPENDED ? 0 : portMAX_DELAY;
        while (xSemaphoreTakeRecursive(libc_mutex, wait) != pdPASS) {
            if (wait == 0) {
                __builtin_trap();
            }
        }
    }
}

void __wrap___retarget_lock_release_recursive(_LOCK_T lock) {
    if (lock != &__lock___libc_recursive_mutex) {
        __real___retarget_lock_release_recursive(lock);
    } else if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xSemaphoreGiveRecursive(libc_mutex);
    }
}

#elif defined(__NEWLIB__)
#include <reent.h>

// Newlib can acquire the malloc lock recursively. Scheduler suspension nests
// and allows allocations during start-up without creating another heap object.
__attribute__((weak)) void __malloc_lock(struct _reent *unused) {
    (void)unused;
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        vTaskSuspendAll();
    }
}

__attribute__((weak)) void __malloc_unlock(struct _reent *unused) {
    (void)unused;
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        (void)xTaskResumeAll();
    }
}
#endif

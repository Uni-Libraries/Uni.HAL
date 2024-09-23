#
# Library
#

add_library(freertos_kernel STATIC)

#
# Sources Common
#

target_sources(freertos_kernel PRIVATE
    "src/croutine.c"
    "src/event_groups.c"
    "src/list.c"
    "src/queue.c"
    "src/stream_buffer.c"
    "src/tasks.c"
    "src/timers.c"


    "src_hooks/freertos_assert.c"
    "src_hooks/freertos_idle.c"
    "src_hooks/freertos_printf.c"
    "src_hooks/freertos_stack.c"

    "src_memmng/heap_4.c"
)

target_sources(freertos_kernel INTERFACE
    "src_hooks/freertos_alloc.c"
)

target_include_directories(freertos_kernel PUBLIC "include")
target_include_directories(freertos_kernel PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/freertos_config/")



#
# Sources Ports
#

if(UNI_HAL_TARGET_MCU STREQUAL "PC")
    if(WIN32)
        target_include_directories(freertos_kernel PUBLIC "src_port/pc_windows")
    else()
        target_include_directories(freertos_kernel PUBLIC "src_port/pc_posix")
        target_sources(freertos_kernel PRIVATE "src_port/pc_posix/port.c")
        target_sources(freertos_kernel PRIVATE "src_port/pc_posix/utils/wait_for_event.c")
    endif()
else()
    target_sources(freertos_kernel INTERFACE "src_port/arm_cm4f/port.c")
    target_include_directories(freertos_kernel PUBLIC "src_port/arm_cm4f")
endif()



#
# Libraries
#

if(NOT UNI_HAL_TARGET_MCU STREQUAL "PC")
    target_link_libraries(freertos_kernel PUBLIC segger_rtt)
    target_link_libraries(freertos_kernel PUBLIC segger_systemview_freertos)
endif()



#
# Properties
#

# language standard
if(MSVC)
    target_compile_features(freertos_kernel PRIVATE c_std_17)
else()
    target_compile_features(freertos_kernel PRIVATE c_std_23)
endif()

# compile options
target_compile_options(freertos_kernel PRIVATE -Wno-unused-parameter)



#
# Config
#

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/freertos_config/FreeRTOSConfig.h"
        "#ifndef FREERTOS_CONFIG_H                                                      \n"
        "#define FREERTOS_CONFIG_H                                                      \n"
        "                                                                               \n"
        "#include <stdint.h>                                                            \n"
        "                                                                               \n"
        "extern uint32_t SystemCoreClock;                                               \n"
        "extern void vLoggingPrintf( const char * pcFormat, ... );                      \n"
        "extern void vAssertCalled(uint32_t ulLine, const char *pcFile);                \n"
        "                                                                               \n"
        "#define configTOTAL_HEAP_SIZE                   (300U * 1024U)                 \n"
        "#define configAPPLICATION_ALLOCATED_HEAP        (${UNI_HAL_RTOS_HEAP_APP})     \n"
        "                                                                               \n"
        "#define configCPU_CLOCK_HZ                      (SystemCoreClock)              \n"
        "                                                                               \n"
        "#define configTICK_RATE_HZ                      (1000)                         \n"
        "#define configTICK_TYPE_WIDTH_IN_BITS           (TICK_TYPE_WIDTH_32_BITS)      \n"
        "                                                                               \n"
        "#define configPRIO_BITS                         (4U)                           \n"
        "#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (4U << (8U - configPRIO_BITS)) \n"
        "                                                                               \n"
        "#define configNUM_THREAD_LOCAL_STORAGE_POINTERS (4)                            \n"
        "                                                                               \n"
        "#define configMAX_PRIORITIES                    (5)                            \n"
        "#define configUSE_PREEMPTION                    (1)                            \n"
        "#define configUSE_IDLE_HOOK                     (0)                            \n"
        "#define configUSE_TICK_HOOK                     (0)                            \n"
        "#define configUSE_MUTEXES                       (1)                            \n"
        "#define configUSE_RECURSIVE_MUTEXES             (1)                            \n"
        "#define configUSE_COUNTING_SEMAPHORES           (1)                            \n"
        "#define configSUPPORT_STATIC_ALLOCATION         (1)                            \n"
        "#define configSUPPORT_DYNAMIC_ALLOCATION        (1)                            \n"
        "#define configCHECK_FOR_STACK_OVERFLOW          (2)                            \n"
        "#define configRECORD_STACK_HIGH_ADDRESS         (1)                            \n"
        "                                                                               \n"
        "#define INCLUDE_vTaskDelay                      (1)                            \n"
        "#define INCLUDE_vTaskDelete                     (1)                            \n"
        "#define INCLUDE_vTaskSuspend                    (1)                            \n"
        "#define INCLUDE_xTaskGetSchedulerState          (1)                            \n"
        "#define INCLUDE_xTaskGetIdleTaskHandle          (1)                            \n"
        "                                                                               \n"
        "#define configPRINTF( X )                       vLoggingPrintf X               \n"
        "                                                                               \n"
        "#define xPortPendSVHandler                      PendSV_Handler                 \n"
        "#define vPortSVCHandler                         SVC_Handler                    \n"
        "                                                                               \n"
        "#define configASSERT(x)  if((x) == 0){vAssertCalled(__LINE__, __FILE__);}      \n"
        "                                                                               \n"
)

if(UNI_HAL_TARGET_MCU STREQUAL "PC")
    file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/freertos_config/FreeRTOSConfig.h"
        "// port specific: PC                                                           \n"
        "#define configMINIMAL_STACK_SIZE                (16384U)                       \n"
        "#define configUSE_PORT_OPTIMISED_TASK_SELECTION (0)                            \n"
        "                                                                               \n"
    )
else()
    file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/freertos_config/FreeRTOSConfig.h"
        "// port specific: ARM Cortex-M                                                 \n"
        "#define configMINIMAL_STACK_SIZE                (512U)                         \n"
        "#define configUSE_PORT_OPTIMISED_TASK_SELECTION (1)                            \n"
        "#include \"SEGGER_SYSVIEW_FreeRTOS.h\"                                         \n"
        "                                                                               \n"
    )
endif()

file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/freertos_config/FreeRTOSConfig.h"
        "#endif                                                                         \n"
        "                                                                               \n"
)

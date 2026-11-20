cmake_minimum_required(VERSION 3.29)
project(pc_minimal_task LANGUAGES C)

# Exercise the shipped PC stack and heap sizes even when the enclosing application
# supplies a different FreeRTOSConfig.h or an application-owned heap.
set(UNI_HAL_TARGET_MCU PC)
set(UNI_HAL_RTOS_CONFIG_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/../config/freertos_dynamic_only.h")
add_subdirectory(../../3rdparty/freertos_kernel kernel)

add_executable(minimal_task minimal_task.c)
target_link_libraries(minimal_task PRIVATE freertos_kernel)
enable_testing()
add_test(NAME run_minimal_task COMMAND minimal_task)
set_tests_properties(run_minimal_task PROPERTIES TIMEOUT 10)

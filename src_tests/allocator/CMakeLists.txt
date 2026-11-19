cmake_minimum_required(VERSION 3.29)
project(uni_hal_allocator_regression LANGUAGES C CXX ASM)

if(UNI_HAL_TARGET_MCU STREQUAL "PC" OR NOT UNI_HAL_TARGET_MCU)
    message(FATAL_ERROR "Configure this firmware test with an MCU and its toolchain")
endif()

add_subdirectory(../.. uni.hal)
include(../../uni.hal.cmake)
uni_hal_add_executable(allocator_regression)
target_sources(allocator_regression PRIVATE main.c)
target_compile_options(allocator_regression PRIVATE -fno-builtin)
set(map_file "${CMAKE_CURRENT_BINARY_DIR}/allocator.map")
target_link_options(allocator_regression PRIVATE "LINKER:-Map,${map_file},--cref")
set_property(TARGET allocator_regression APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/check_providers.cmake")
add_custom_command(TARGET allocator_regression POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DMAP_FILE=${map_file}" "-DBIN_FILE=${CMAKE_CURRENT_BINARY_DIR}/allocator_regression.bin"
        "-DPICOLIBC=${UNI_HAL_HAS_PICOLIBC}" "-DMCU=${UNI_HAL_TARGET_MCU}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_providers.cmake"
    VERBATIM)

# The QEMU F405 board has the same Cortex-M4 and flash base, with 128 KiB of SRAM.
# The allocator test uses the CPU and FreeRTOS, without accessing STM32 peripherals.
option(UNI_HAL_TEST_ALLOCATOR_QEMU "Run the allocator on an emulated Cortex-M4" OFF)
if(UNI_HAL_TEST_ALLOCATOR_QEMU)
    if(NOT UNI_HAL_TARGET_MCU STREQUAL "STM32L496")
        message(FATAL_ERROR "The allocator QEMU test uses STM32L496")
    endif()
    find_program(QEMU_SYSTEM_ARM qemu-system-arm NO_CMAKE_FIND_ROOT_PATH REQUIRED)
    enable_testing()
    uni_hal_add_executable(allocator_qemu)
    target_sources(allocator_qemu PRIVATE main.c qemu_main.c ../io/freertos_buffers.c)
    target_compile_options(allocator_qemu PRIVATE -fno-builtin)
    target_link_options(allocator_qemu PRIVATE
        "LINKER:--defsym,_estack=0x20020000" "LINKER:--wrap=main")
    add_test(NAME allocator_cortex_m4 COMMAND "${QEMU_SYSTEM_ARM}" -M netduinoplus2
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:allocator_qemu>)
    set_tests_properties(allocator_cortex_m4 PROPERTIES TIMEOUT 15)
endif()

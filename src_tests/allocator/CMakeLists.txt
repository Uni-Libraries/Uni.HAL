cmake_minimum_required(VERSION 3.29)
project(uni_hal_allocator_regression LANGUAGES C CXX ASM)

if(UNI_HAL_TARGET_MCU STREQUAL "PC" OR NOT UNI_HAL_TARGET_MCU)
    message(FATAL_ERROR "Configure this firmware test with an MCU and its toolchain")
endif()

add_subdirectory(../.. uni.hal)
include(../../uni.hal.cmake)
add_subdirectory(../public_api public_api)
uni_hal_add_executable(allocator_regression)
target_sources(allocator_regression PRIVATE main.c)
target_compile_options(allocator_regression PRIVATE -fno-builtin)
set(map_file "$<TARGET_FILE_DIR:allocator_regression>/allocator.map")
target_link_options(allocator_regression PRIVATE "LINKER:-Map,${map_file},--cref")
set_property(TARGET allocator_regression APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/check_providers.cmake")
add_custom_command(TARGET allocator_regression POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DMAP_FILE=${map_file}"
        "-DBIN_FILE=$<PATH:REPLACE_EXTENSION,LAST_ONLY,$<TARGET_FILE:allocator_regression>,.bin>"
        "-DPICOLIBC=${UNI_HAL_HAS_PICOLIBC}" "-DMCU=${UNI_HAL_TARGET_MCU}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_providers.cmake"
    VERBATIM)

# The QEMU F405 board has the same Cortex-M4 and flash base, with 128 KiB of SRAM.
# The allocator test uses the CPU and FreeRTOS, without accessing STM32 peripherals.
option(UNI_HAL_TEST_ALLOCATOR_QEMU "Run the allocator on an emulated Cortex-M4" OFF)
option(UNI_HAL_TEST_STM32_QEMU "Run the STM32 peripheral regressions on an emulated Cortex-M" OFF)
if(UNI_HAL_TEST_ALLOCATOR_QEMU OR UNI_HAL_TEST_STM32_QEMU)
    find_program(QEMU_SYSTEM_ARM qemu-system-arm NO_CMAKE_FIND_ROOT_PATH REQUIRED)
    enable_testing()
endif()
if(UNI_HAL_TEST_ALLOCATOR_QEMU)
    if(NOT UNI_HAL_TARGET_MCU STREQUAL "STM32L496")
        message(FATAL_ERROR "The allocator QEMU test uses STM32L496")
    endif()
    uni_hal_add_executable(allocator_qemu)
    target_sources(allocator_qemu PRIVATE main.c qemu_main.c freertos_buffers.c io_definition.cpp tunnel_irq.c receive_irq.c)
    target_compile_features(allocator_qemu PRIVATE cxx_std_20)
    # The C++ aggregate probe uses the same C runtime and startup as this firmware.
    set_target_properties(allocator_qemu PROPERTIES CXX_SCAN_FOR_MODULES OFF LINKER_LANGUAGE C)
    target_compile_definitions(allocator_qemu PRIVATE UNI_HAL_TEST_SYSTICK_SLEEP)
    target_compile_options(allocator_qemu PRIVATE -fno-builtin)
    target_link_options(allocator_qemu PRIVATE
        "LINKER:--defsym,_estack=0x20020000" "LINKER:--wrap=main")
    add_test(NAME allocator_cortex_m4 COMMAND "${QEMU_SYSTEM_ARM}" -M netduinoplus2
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:allocator_qemu>)
    set_tests_properties(allocator_cortex_m4 PROPERTIES TIMEOUT 15)

    foreach(policy IN ITEMS default application)
        set(target oom_${policy}_qemu)
        uni_hal_add_executable(${target})
        target_sources(${target} PRIVATE oom.c)
        if(policy STREQUAL "application")
            target_compile_definitions(${target} PRIVATE TEST_APPLICATION_OOM_HOOK)
        endif()
        target_link_options(${target} PRIVATE "LINKER:--defsym,_estack=0x20020000")
        add_test(NAME oom_${policy} COMMAND "${QEMU_SYSTEM_ARM}" -M netduinoplus2
            -nographic -monitor none -serial none -semihosting-config enable=on,target=native
            -kernel $<TARGET_FILE:${target}>)
        set_tests_properties(oom_${policy} PROPERTIES TIMEOUT 10)
    endforeach()

    uni_hal_add_executable(task_libc_qemu)
    target_sources(task_libc_qemu PRIVATE task_errno.c)
    target_link_options(task_libc_qemu PRIVATE "LINKER:--defsym,_estack=0x20020000")
    add_test(NAME task_libc_cortex_m4 COMMAND "${QEMU_SYSTEM_ARM}" -M netduinoplus2
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:task_libc_qemu>)
    set_tests_properties(task_libc_cortex_m4 PROPERTIES TIMEOUT 15)

    uni_hal_add_executable(i2c_polling_qemu)
    target_sources(i2c_polling_qemu PRIVATE ../i2c_timeout/vendor_qemu.c)
    target_link_options(i2c_polling_qemu PRIVATE
        "LINKER:--defsym,_estack=0x20020000" "LINKER:--wrap=HAL_GetTick")
    add_test(NAME i2c_polling_cortex_m4 COMMAND "${QEMU_SYSTEM_ARM}" -M netduinoplus2
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:i2c_polling_qemu>)
    set_tests_properties(i2c_polling_cortex_m4 PROPERTIES TIMEOUT 15)

    # Exercise H743 bus arbitration with the real Cortex-M scheduler and modeled I2C handles.
    uni_hal_add_executable(i2c_h743_tasks_qemu)
    target_sources(i2c_h743_tasks_qemu PRIVATE ../i2c_timeout/model.c ../i2c_timeout/tasks_qemu.c
        ../../src/i2c/uni_hal_i2c_stm32h7.c)
    target_include_directories(i2c_h743_tasks_qemu PRIVATE ../i2c_h7/fakes ../i2c)
    target_compile_definitions(i2c_h743_tasks_qemu PRIVATE TEST_I2C_TASKS_QEMU)
    target_link_options(i2c_h743_tasks_qemu PRIVATE "LINKER:--defsym,_estack=0x20020000"
        "LINKER:--wrap=uni_hal_rcc_clk_set,--wrap=uni_hal_rcc_clksrc_set")
    add_test(NAME i2c_h743_bus_ownership COMMAND "${QEMU_SYSTEM_ARM}" -M netduinoplus2
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:i2c_h743_tasks_qemu>)
    set_tests_properties(i2c_h743_bus_ownership PROPERTIES TIMEOUT 10)

    uni_hal_add_executable(i2c_l496_priority_qemu)
    target_sources(i2c_l496_priority_qemu PRIVATE ../i2c_transfer/priority_qemu.c ../i2c_timeout/tasks_qemu.c)
    target_link_options(i2c_l496_priority_qemu PRIVATE
        "LINKER:--defsym,_estack=0x20020000" "LINKER:--wrap=uni_hal_rcc_clk_set")
    add_test(NAME i2c_l496_irq_priority COMMAND "${QEMU_SYSTEM_ARM}" -M netduinoplus2
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:i2c_l496_priority_qemu>)
    set_tests_properties(i2c_l496_irq_priority PROPERTIES TIMEOUT 10)

    add_subdirectory(../rtt rtt)

    uni_hal_add_executable(dma_l4_qemu)
    target_sources(dma_l4_qemu PRIVATE ../dma_l4/channel_irq_qemu.c)
    target_link_options(dma_l4_qemu PRIVATE
        "LINKER:--defsym,_estack=0x20020000" "LINKER:--wrap=uni_hal_rcc_clk_set")
    add_test(NAME dma_l496_channel_irqs COMMAND "${QEMU_SYSTEM_ARM}" -M netduinoplus2
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:dma_l4_qemu>)
    set_tests_properties(dma_l496_channel_irqs PROPERTIES TIMEOUT 15)
endif()

if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    uni_hal_add_executable(i2c_h7_recovery_link)
    target_sources(i2c_h7_recovery_link PRIVATE ../i2c_h7/link_recover.c)
endif()

if(UNI_HAL_TEST_STM32_QEMU)
    add_subdirectory(../mco mco)
    add_subdirectory(../gpio gpio)
    add_subdirectory(../dwt_stm32 dwt_stm32)
    add_subdirectory(../rtc_stm32 rtc_stm32)
    add_subdirectory(../rcc_stm32 rcc_stm32)
    add_subdirectory(../pwr_stm32 pwr_stm32)
    add_subdirectory(../iwdg iwdg)
    add_subdirectory(../rng_stm32 rng_stm32)
    add_subdirectory(../crc_stm32 crc_stm32)
    add_subdirectory(../adc adc)
    add_subdirectory(../can_tx can_tx)
    add_subdirectory(../spi spi)
    add_subdirectory(../i2c_clock i2c_clock)
    add_subdirectory(../i2c_arguments i2c_arguments)
    add_subdirectory(../uart_stm32 uart_stm32)
    if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
        add_subdirectory(../crc_h7 crc_h7)
        add_subdirectory(../tim_h7 tim_h7)
        add_subdirectory(../uart_h7 uart_h7)
        add_subdirectory(../flash_h7 flash_h7)
        add_subdirectory(../rcc_h7 rcc_h7)
    elseif(UNI_HAL_TARGET_MCU STREQUAL "STM32L496")
        add_subdirectory(../spi_l4 spi_l4)
        add_subdirectory(../tim_l4 tim_l4)
        add_subdirectory(../uart_l4 uart_l4)
    endif()
endif()

option(UNI_HAL_TEST_ZYNQ_QEMU "Run the Zynq BSP and FreeRTOS on an emulated Cortex-A9" OFF)
if(UNI_HAL_TEST_ZYNQ_QEMU)
    if(NOT UNI_HAL_TARGET_MCU STREQUAL "ZYNQ7000")
        message(FATAL_ERROR "The Cortex-A9 IRQ test uses ZYNQ7000")
    endif()
    find_program(QEMU_SYSTEM_ARM qemu-system-arm NO_CMAKE_FIND_ROOT_PATH REQUIRED)
    enable_testing()
    uni_hal_add_executable(irq_nesting_zynq)
    target_sources(irq_nesting_zynq PRIVATE ../irq_zynq/nesting.c)
    add_test(NAME irq_nesting_zynq COMMAND "${QEMU_SYSTEM_ARM}" -M xilinx-zynq-a9
        -nographic -nodefaults -monitor none -serial none -semihosting-config enable=on,target=native
        -device "loader,file=$<TARGET_FILE:irq_nesting_zynq>,cpu-num=0")
    set_tests_properties(irq_nesting_zynq PROPERTIES TIMEOUT 15)
endif()

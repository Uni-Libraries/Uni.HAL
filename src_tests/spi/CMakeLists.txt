if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(spi_machine mps2-an500)
else()
    set(spi_machine mps2-an386)
endif()

add_executable(spi_stalled_tick_qemu stalled_tick.c)
target_link_libraries(spi_stalled_tick_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(spi_stalled_tick_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME spi_stalled_tick COMMAND "${QEMU_SYSTEM_ARM}" -M ${spi_machine}
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:spi_stalled_tick_qemu>)
set_tests_properties(spi_stalled_tick PROPERTIES TIMEOUT 10)

if(NOT UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    return()
endif()

add_executable(spi_qemu ownership.c)
target_link_libraries(spi_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(spi_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME spi_dma_ownership COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an500
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:spi_qemu>)
set_tests_properties(spi_dma_ownership PROPERTIES TIMEOUT 10)

add_executable(spi_kernel_clock_qemu kernel_clock.c)
target_link_libraries(spi_kernel_clock_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(spi_kernel_clock_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME spi_h743_shared_clock COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an500
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:spi_kernel_clock_qemu>)
set_tests_properties(spi_h743_shared_clock PROPERTIES TIMEOUT 10)

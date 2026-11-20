if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(uart_machine mps2-an500)
else()
    set(uart_machine mps2-an386)
endif()

add_executable(uart_enable_stalled_tick_qemu enable_stalled_tick.c)
target_link_libraries(uart_enable_stalled_tick_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(uart_enable_stalled_tick_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME uart_enable_stalled_tick COMMAND "${QEMU_SYSTEM_ARM}" -M ${uart_machine}
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:uart_enable_stalled_tick_qemu>)
set_tests_properties(uart_enable_stalled_tick PROPERTIES TIMEOUT 10)

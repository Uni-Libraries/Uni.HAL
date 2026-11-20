add_executable(uart_h7_qemu completion.c)
target_link_libraries(uart_h7_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(uart_h7_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME uart_h743_completion COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an500
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:uart_h7_qemu>)
set_tests_properties(uart_h743_completion PROPERTIES TIMEOUT 10)

function(add_uart_clock_test name source)
    add_executable(${name}_qemu ${source})
    target_link_libraries(${name}_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(${name}_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME ${name} COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an500
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:${name}_qemu>)
    set_tests_properties(${name} PROPERTIES TIMEOUT 10)
endfunction()

add_uart_clock_test(uart_h743_kernel_clock kernel_clock.c)
add_uart_clock_test(uart_h743_shared_clock shared_clock.c)

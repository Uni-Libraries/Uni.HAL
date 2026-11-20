add_executable(tim_capture_units_qemu capture_units.c)
target_link_libraries(tim_capture_units_qemu PRIVATE uni.hal)
target_link_options(tim_capture_units_qemu PRIVATE "LINKER:--wrap=uni_hal_rcc_clk_get_freq")
uni_hal_set_linker_file(tim_capture_units_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME tim_l496_capture_units COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an386
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:tim_capture_units_qemu>)
set_tests_properties(tim_l496_capture_units PROPERTIES TIMEOUT 10)

function(add_capture_irq_test name source)
    add_executable(${name}_qemu model.c ${source})
    target_link_libraries(${name}_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(${name}_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME ${name} COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an386
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:${name}_qemu>)
    set_tests_properties(${name} PROPERTIES TIMEOUT 10)
endfunction()

add_capture_irq_test(tim_l496_capture_wrap capture_wrap.c)

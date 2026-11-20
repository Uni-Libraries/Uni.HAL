function(add_rcc_h7_test name source)
    add_executable(${name}_qemu model.c ${source})
    target_link_libraries(${name}_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(${name}_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME ${name} COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an500
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:${name}_qemu>)
    set_tests_properties(${name} PROPERTIES TIMEOUT 8)
endfunction()

add_rcc_h7_test(rcc_h743_config_ownership ownership.c)
add_rcc_h7_test(rcc_h743_css_recovery recovery.c)
add_rcc_h7_test(rcc_h743_css_stalled_clocks stalled.c)

function(add_pwr_test name source machine)
    add_executable(${name}_qemu ${source})
    target_link_libraries(${name}_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(${name}_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME ${name} COMMAND "${QEMU_SYSTEM_ARM}" -M ${machine}
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:${name}_qemu>)
    set_tests_properties(${name} PROPERTIES TIMEOUT 10)
endfunction()

if(UNI_HAL_TARGET_MCU STREQUAL "STM32L496")
    add_pwr_test(pwr_l496_voltage_ready voltage_l4.c mps2-an386)
endif()

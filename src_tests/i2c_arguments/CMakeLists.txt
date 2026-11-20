if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(port h743)
    set(machine mps2-an500)
else()
    set(port l496)
    set(machine mps2-an386)
endif()

function(add_i2c_argument_test name source)
    add_executable(${name}_qemu ${source})
    target_link_libraries(${name}_qemu PRIVATE uni.hal)
    target_link_options(${name}_qemu PRIVATE "LINKER:--wrap=HAL_GetTick")
    uni_hal_set_linker_file(${name}_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME ${name} COMMAND "${QEMU_SYSTEM_ARM}" -M ${machine}
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:${name}_qemu>)
    set_tests_properties(${name} PROPERTIES TIMEOUT 10)
endfunction()

add_i2c_argument_test(i2c_${port}_device_address device_address.c)
if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    add_i2c_argument_test(i2c_h743_memory_address_width memory_width.c)
endif()

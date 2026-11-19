if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(adc_machine mps2-an500)
else()
    set(adc_machine mps2-an386)
endif()

add_executable(adc_qemu model.c bounds.c)
target_link_libraries(adc_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(adc_qemu "${CMAKE_CURRENT_SOURCE_DIR}/mps2.ld")
add_test(NAME adc_configuration COMMAND "${QEMU_SYSTEM_ARM}" -M ${adc_machine}
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:adc_qemu>)
set_tests_properties(adc_configuration PROPERTIES TIMEOUT 10)

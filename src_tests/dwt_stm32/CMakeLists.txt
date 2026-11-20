if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(dwt_machine mps2-an500)
else()
    set(dwt_machine mps2-an386)
endif()

add_executable(dwt_delay_qemu delay.c)
target_link_libraries(dwt_delay_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(dwt_delay_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME dwt_delay_wrap COMMAND "${QEMU_SYSTEM_ARM}" -M ${dwt_machine}
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:dwt_delay_qemu>)
set_tests_properties(dwt_delay_wrap PROPERTIES TIMEOUT 10)

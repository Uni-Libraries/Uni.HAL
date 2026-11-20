if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(rtc_machine mps2-an500)
else()
    set(rtc_machine mps2-an386)
endif()

add_executable(rtc_retained_hours_qemu retained_hours.c)
target_link_libraries(rtc_retained_hours_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(rtc_retained_hours_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME rtc_retained_hours COMMAND "${QEMU_SYSTEM_ARM}" -M ${rtc_machine}
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:rtc_retained_hours_qemu>)
set_tests_properties(rtc_retained_hours PROPERTIES TIMEOUT 10)

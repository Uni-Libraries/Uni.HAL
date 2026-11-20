if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(rtc_machine mps2-an500)
else()
    set(rtc_machine mps2-an386)
endif()

foreach(scenario IN ITEMS retained_hours calendar_update calendar_read clock_readback clock_selection)
    set(target rtc_${scenario})
    add_executable(${target}_qemu ${scenario}.c)
    target_link_libraries(${target}_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(${target}_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME ${target} COMMAND "${QEMU_SYSTEM_ARM}" -M ${rtc_machine}
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:${target}_qemu>)
    set_tests_properties(${target} PROPERTIES TIMEOUT 10)
endforeach()

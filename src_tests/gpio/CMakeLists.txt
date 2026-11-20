if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(gpio_machine mps2-an500)
else()
    set(gpio_machine mps2-an386)
endif()

add_executable(gpio_exti_handoff_qemu exti_handoff.c)
target_link_libraries(gpio_exti_handoff_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(gpio_exti_handoff_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME gpio_exti_handoff COMMAND "${QEMU_SYSTEM_ARM}" -M ${gpio_machine}
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:gpio_exti_handoff_qemu>)
set_tests_properties(gpio_exti_handoff PROPERTIES TIMEOUT 10)

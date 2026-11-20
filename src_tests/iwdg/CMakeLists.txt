if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(iwdg_machine mps2-an500)
else()
    set(iwdg_machine mps2-an386)
endif()

add_executable(iwdg_deinit_qemu deinit.c)
target_link_libraries(iwdg_deinit_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(iwdg_deinit_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME iwdg_deinit COMMAND "${QEMU_SYSTEM_ARM}" -M ${iwdg_machine}
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:iwdg_deinit_qemu>)
set_tests_properties(iwdg_deinit PROPERTIES TIMEOUT 10)

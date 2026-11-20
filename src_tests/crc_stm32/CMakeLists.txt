if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(crc_machine mps2-an500)
else()
    set(crc_machine mps2-an386)
endif()
add_executable(crc_contexts_qemu contexts.c)
target_link_libraries(crc_contexts_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(crc_contexts_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME crc_independent_contexts COMMAND "${QEMU_SYSTEM_ARM}" -M ${crc_machine}
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:crc_contexts_qemu>)
set_tests_properties(crc_independent_contexts PROPERTIES TIMEOUT 10)

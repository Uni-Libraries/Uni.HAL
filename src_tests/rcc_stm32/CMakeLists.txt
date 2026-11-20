if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(rcc_machine mps2-an500)
else()
    set(rcc_machine mps2-an386)
endif()

add_executable(rcc_gate_preemption_qemu gate_preemption.c)
target_link_libraries(rcc_gate_preemption_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(rcc_gate_preemption_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME rcc_gate_preemption COMMAND "${QEMU_SYSTEM_ARM}" -M ${rcc_machine}
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:rcc_gate_preemption_qemu>)
set_tests_properties(rcc_gate_preemption PROPERTIES TIMEOUT 10)

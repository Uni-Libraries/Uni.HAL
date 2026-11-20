if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    add_executable(mco_qemu h743.c)
    set(mco_machine mps2-an500)
else()
    add_executable(mco_qemu l496.c)
    set(mco_machine mps2-an386)
endif()
target_link_libraries(mco_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(mco_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME rcc_mco_enable COMMAND "${QEMU_SYSTEM_ARM}" -M ${mco_machine}
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:mco_qemu>)
set_tests_properties(rcc_mco_enable PROPERTIES TIMEOUT 10)

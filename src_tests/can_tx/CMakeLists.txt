if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(can_machine mps2-an500)
else()
    set(can_machine mps2-an386)
endif()

add_executable(can_tx_qemu reservation.c)
target_link_libraries(can_tx_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(can_tx_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME can_tx_reservation COMMAND "${QEMU_SYSTEM_ARM}" -M ${can_machine}
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:can_tx_qemu>)
set_tests_properties(can_tx_reservation PROPERTIES TIMEOUT 10)

if(UNI_HAL_TARGET_MCU STREQUAL "STM32L496")
    uni_hal_add_executable(can_filters_l496_qemu)
    target_sources(can_filters_l496_qemu PRIVATE filters_l496.c ../i2c_timeout/tasks_qemu.c)
    target_link_options(can_filters_l496_qemu PRIVATE
        "LINKER:--defsym,_estack=0x20020000" "LINKER:--wrap=uni_hal_rcc_clk_set")
    add_test(NAME can_l496_shared_filters COMMAND "${QEMU_SYSTEM_ARM}" -M netduinoplus2
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:can_filters_l496_qemu>)
    set_tests_properties(can_l496_shared_filters PROPERTIES TIMEOUT 10)
endif()

if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    add_executable(can_h743_error_snapshot_qemu error_snapshot_h743.c)
    target_link_libraries(can_h743_error_snapshot_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(can_h743_error_snapshot_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME can_h743_error_snapshot COMMAND "${QEMU_SYSTEM_ARM}" -M ${can_machine}
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:can_h743_error_snapshot_qemu>)
    set_tests_properties(can_h743_error_snapshot PROPERTIES TIMEOUT 10)

    add_executable(can_clock_h743_qemu clock_h743.c)
    target_link_libraries(can_clock_h743_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(can_clock_h743_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME can_h743_shared_clock COMMAND "${QEMU_SYSTEM_ARM}" -M ${can_machine}
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:can_clock_h743_qemu>)
    set_tests_properties(can_h743_shared_clock PROPERTIES TIMEOUT 10)

    add_executable(can_timing_h743_qemu timing_h743.c)
    target_link_libraries(can_timing_h743_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(can_timing_h743_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME can_h743_timing COMMAND "${QEMU_SYSTEM_ARM}" -M ${can_machine}
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:can_timing_h743_qemu>)
    set_tests_properties(can_h743_timing PROPERTIES TIMEOUT 10)
endif()

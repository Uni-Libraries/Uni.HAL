if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    add_executable(i2c_memory_width_qemu memory_width.c)
    target_link_libraries(i2c_memory_width_qemu PRIVATE uni.hal)
    target_link_options(i2c_memory_width_qemu PRIVATE "LINKER:--wrap=HAL_GetTick")
    uni_hal_set_linker_file(i2c_memory_width_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME i2c_h743_memory_address_width COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an500
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:i2c_memory_width_qemu>)
    set_tests_properties(i2c_h743_memory_address_width PROPERTIES TIMEOUT 10)
endif()

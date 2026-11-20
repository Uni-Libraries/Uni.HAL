add_executable(i2c_l496_kernel_clock_qemu l496.c)
target_link_libraries(i2c_l496_kernel_clock_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(i2c_l496_kernel_clock_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME i2c_l496_kernel_clock COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an386
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:i2c_l496_kernel_clock_qemu>)
set_tests_properties(i2c_l496_kernel_clock PROPERTIES TIMEOUT 10)

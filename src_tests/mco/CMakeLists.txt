add_executable(mco_qemu l496.c)
target_link_libraries(mco_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(mco_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME rcc_mco_enable COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an386
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:mco_qemu>)
set_tests_properties(rcc_mco_enable PROPERTIES TIMEOUT 10)

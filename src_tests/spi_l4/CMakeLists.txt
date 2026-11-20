add_executable(spi_l4_qemu config_qemu.c)
target_link_libraries(spi_l4_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(spi_l4_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME spi_l496_config COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an386
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:spi_l4_qemu>)
set_tests_properties(spi_l496_config PROPERTIES TIMEOUT 10)

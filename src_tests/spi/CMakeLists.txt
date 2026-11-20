add_executable(spi_qemu ownership.c)
target_link_libraries(spi_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(spi_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
add_test(NAME spi_dma_ownership COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an500
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:spi_qemu>)
set_tests_properties(spi_dma_ownership PROPERTIES TIMEOUT 10)

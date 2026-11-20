add_executable(crc_h7_qemu append.c)
target_link_libraries(crc_h7_qemu PRIVATE uni.hal)
uni_hal_set_linker_file(crc_h7_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
# Trap an invalid shift when packing a word, even if the emitted bits happen to match.
target_compile_options(crc_h7_qemu PRIVATE -fsanitize=shift
    $<$<C_COMPILER_ID:GNU>:-fsanitize-undefined-trap-on-error>
    $<$<C_COMPILER_ID:Clang>:-fsanitize-trap=shift>)
add_test(NAME crc_h743_append COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an500
    -nographic -monitor none -serial none -semihosting-config enable=on,target=native
    -kernel $<TARGET_FILE:crc_h7_qemu>)
set_tests_properties(crc_h743_append PROPERTIES TIMEOUT 10)

function(add_flash_h7_test name source)
    add_executable(${name}_qemu ${source})
    target_link_libraries(${name}_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(${name}_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME ${name} COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an500
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:${name}_qemu>)
    set_tests_properties(${name} PROPERTIES TIMEOUT 10)
endfunction()

add_flash_h7_test(flash_h743_init init.c)
add_flash_h7_test(flash_h743_irq_priority init.c)
target_compile_definitions(flash_h743_irq_priority_qemu PRIVATE TEST_STRICT_IRQ_CEILING)
add_flash_h7_test(flash_h743_ownership ownership.c)
add_flash_h7_test(flash_h743_write write.c)
add_flash_h7_test(flash_h743_axi_latency latency.c)
add_flash_h7_test(flash_h743_write_cache write_cache.c)

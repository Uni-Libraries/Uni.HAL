foreach(scenario IN ITEMS config completion overrun)
    set(target spi_l496_${scenario})
    add_executable(${target}_qemu ${scenario}_qemu.c)
    target_link_libraries(${target}_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(${target}_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME ${target} COMMAND "${QEMU_SYSTEM_ARM}" -M mps2-an386
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:${target}_qemu>)
    set_tests_properties(${target} PROPERTIES TIMEOUT 10)
endforeach()

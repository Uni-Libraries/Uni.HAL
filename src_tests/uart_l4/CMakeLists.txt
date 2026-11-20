foreach(port IN ITEMS usart lpuart)
    set(target uart_l496_${port}_completion)
    uni_hal_add_executable(${target}_qemu)
    target_sources(${target}_qemu PRIVATE completion.c)
    if(port STREQUAL "lpuart")
        target_compile_definitions(${target}_qemu PRIVATE TEST_LPUART)
    endif()
    target_link_options(${target}_qemu PRIVATE "LINKER:--defsym,_estack=0x20020000")
    add_test(NAME ${target} COMMAND "${QEMU_SYSTEM_ARM}" -M netduinoplus2
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:${target}_qemu>)
    set_tests_properties(${target} PROPERTIES TIMEOUT 10)
endforeach()

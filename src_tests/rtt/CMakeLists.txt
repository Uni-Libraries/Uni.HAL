function(add_rtt_test name source)
    uni_hal_add_executable(${name}_qemu)
    target_sources(${name}_qemu PRIVATE timer.c ${source})
    target_link_options(${name}_qemu PRIVATE "LINKER:--defsym,_estack=0x20020000"
        "LINKER:--wrap=uni_hal_tim_init,--wrap=uni_hal_tim_register_callback"
        "LINKER:--wrap=uni_hal_tim_start,--wrap=uni_hal_tim_stop")
    add_test(NAME ${name} COMMAND "${QEMU_SYSTEM_ARM}" -M netduinoplus2
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:${name}_qemu>)
    set_tests_properties(${name} PROPERTIES TIMEOUT 10)
endfunction()

add_rtt_test(rtt_stdio_init init.c)
add_rtt_test(rtt_stdio_tx transmit.c)
add_rtt_test(rtt_stdio_preserve_buffers preserve_buffers.c)
add_rtt_test(rtt_stdio_buffer_indices buffer_indices.c)

add_rtt_test(rtt_stdio_empty_read empty_read.c)
target_link_options(rtt_stdio_empty_read_qemu PRIVATE "LINKER:--wrap=SEGGER_RTT_Read")

add_rtt_test(rtt_stdio_poll_budget poll_budget.c)
target_link_options(rtt_stdio_poll_budget_qemu PRIVATE
    "LINKER:--wrap=SEGGER_RTT_Read,--wrap=SEGGER_RTT_PutCharSkip")

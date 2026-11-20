if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(iwdg_machine mps2-an500)
else()
    set(iwdg_machine mps2-an386)
endif()

foreach(scenario IN ITEMS deinit instance)
    add_executable(iwdg_${scenario}_qemu ${scenario}.c)
    target_link_libraries(iwdg_${scenario}_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(iwdg_${scenario}_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME iwdg_${scenario} COMMAND "${QEMU_SYSTEM_ARM}" -M ${iwdg_machine}
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:iwdg_${scenario}_qemu>)
    set_tests_properties(iwdg_${scenario} PROPERTIES TIMEOUT 10)
endforeach()

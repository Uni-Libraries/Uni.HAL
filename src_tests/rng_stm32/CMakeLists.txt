set(rng_scenarios clock_timeout word_timeout)
if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    set(rng_machine mps2-an500)
    list(APPEND rng_scenarios reinit)
else()
    set(rng_machine mps2-an386)
endif()

foreach(scenario IN LISTS rng_scenarios)
    set(target rng_${scenario})
    add_executable(${target}_qemu ${scenario}.c)
    target_link_libraries(${target}_qemu PRIVATE uni.hal)
    uni_hal_set_linker_file(${target}_qemu "${CMAKE_CURRENT_SOURCE_DIR}/../adc/mps2.ld")
    add_test(NAME ${target} COMMAND "${QEMU_SYSTEM_ARM}" -M ${rng_machine}
        -nographic -monitor none -serial none -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:${target}_qemu>)
    set_tests_properties(${target} PROPERTIES TIMEOUT 10)
endforeach()

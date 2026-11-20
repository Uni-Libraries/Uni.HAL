cmake_minimum_required(VERSION 3.29)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(dir "${CMAKE_CURRENT_BINARY_DIR}/rcc_map_mco_outputs")
file(MAKE_DIRECTORY "${dir}")
file(WRITE "${dir}/clocks.c" [=[
void configure_outputs(void) {
    uni_hal_rcc_stm32_mco_enable(0U, UNI_HAL_RCC_CLKSRC_HSI, 4U);
    uni_hal_rcc_stm32_mco_enable(1, UNI_HAL_RCC_CLKSRC_HSE, 2U);
    uni_hal_rcc_stm32_mco_enable(2, UNI_HAL_RCC_CLKSRC_LSI, 3U);
}
]=])

function(check_map family hsi_mhz mco_mhz)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../../cmake/fun_rcc_map.py"
        --target-name mco --input-file "${dir}/clocks.c" --define "${family}" ${ARGN}
        --dot-file "${dir}/${family}.dot" --png-file "${dir}/${family}.png" --svg-file "${dir}/${family}.svg"
        COMMAND_ERROR_IS_FATAL ANY)
    file(READ "${dir}/${family}.dot" graph)
    string(FIND "${graph}" [["src_hsi" -> "mco1" [label="/4"]] hsi_mco1)
    if(hsi_mco1 EQUAL -1)
        message(FATAL_ERROR "${family} map is missing HSI/4 on MCO1")
    endif()
    string(FIND "${graph}" "\"src_hsi\" [label=\"HSI\\n${hsi_mhz} MHz\"" source_label)
    string(FIND "${graph}" "\"mco1\" [label=\"MCO1\\n${mco_mhz} MHz\"" output_label)
    if(source_label EQUAL -1 OR output_label EQUAL -1)
        message(FATAL_ERROR "${family} map must show HSI ${hsi_mhz} MHz and MCO1 ${mco_mhz} MHz")
    endif()
    if(family STREQUAL STM32H7)
        string(FIND "${graph}" [["src_hse" -> "mco2" [label="/2"]] hse_mco2)
        if(hse_mco2 EQUAL -1)
            message(FATAL_ERROR "STM32H7 map is missing HSE/2 on MCO2")
        endif()
    elseif(graph MATCHES [["mco2"]])
        message(FATAL_ERROR "STM32L4 map contains a second MCO output")
    endif()
endfunction()

check_map(STM32H7 64 16)
check_map(STM32L4 16 4)
check_map(STM32L4 24 6 --define HSI_VALUE=24000000U)
check_map(STM32L4 32 8 --define HSI_VALUE=24000000U --hsi-hz 32000000)

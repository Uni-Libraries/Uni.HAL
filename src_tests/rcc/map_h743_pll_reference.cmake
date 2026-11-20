cmake_minimum_required(VERSION 3.29)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(dir "${CMAKE_CURRENT_BINARY_DIR}/rcc_map_h743_pll_reference")
file(MAKE_DIRECTORY "${dir}")

function(check_reference name family config setup source divider output)
    if(family STREQUAL STM32H7)
        set(mcu stm32h7)
    else()
        set(mcu stm32l4)
    endif()
    set(input "${dir}/${name}.c")
    file(WRITE "${input}" "
#include <stdbool.h>
#include \"rcc/uni_hal_rcc_${mcu}.h\"
#include <${mcu}xx_ll_rcc.h>
const uni_hal_rcc_${mcu}_config_t clocks = { ${config} };
void configure(void) { ${setup} }
")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../../cmake/fun_rcc_map.py"
        --target-name clocks --define "${family}" --hse-hz 8000000 --input-file "${input}"
        --dot-file "${dir}/${name}.dot" --png-file "${dir}/${name}.png" --svg-file "${dir}/${name}.svg"
        COMMAND_ERROR_IS_FATAL ANY)
    file(READ "${dir}/${name}.dot" graph)
    string(FIND "${graph}" "\"${source}\" -> \"pll1\" [label=\"/${divider}\"" edge)
    string(FIND "${graph}" "\"pll1_q\" [label=\"PLL1Q\\n${output}\"" rate)
    if(edge EQUAL -1 OR rate EQUAL -1)
        message(FATAL_ERROR "${name} must use ${source}/${divider} and show PLL1Q at ${output}")
    endif()
    foreach(expected IN LISTS ARGN)
        string(FIND "${graph}" "${expected}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR "${name} is missing '${expected}'")
        endif()
    endforeach()
endfunction()

set(q ".pll[0] = { .m = 4, .n = 100, .q = 4 }")
check_reference(hse STM32H7 ".hse_enable = true, .csi_enable = true, ${q}" "" src_hse 4 "50 MHz")
check_reference(csi STM32H7 ".csi_enable = true, ${q}" "" src_csi 2 "50 MHz")
check_reference(hsi STM32H7 "${q}" "" src_hsi 32 "50 MHz")
check_reference(divided_hsi STM32H7 "${q}" "LL_RCC_HSI_SetDivider(LL_RCC_HSI_DIV2);" src_hsi "2 /16" "50 MHz"
    [=["bus_sysclk" [label="SYSCLK\n32 MHz"]]=] [=["src_hsi" -> "bus_sysclk" [label="/2", penwidth=1.2]]=])
check_reference(non_integral STM32H7 ".csi_enable = true, .pll[0] = { .m = 3, .n = 100, .q = 4 }"
    "" src_csi "?" "?")
check_reference(out_of_range STM32H7 ".pll[0] = { .m = 8, .n = 100, .q = 4 }" "" src_hsi "?" "?")
check_reference(other_pll_failed STM32H7 "${q}, .pll[1] = { .m = 8, .n = 100, .r = 2 }" "" src_hsi 32 "?")
check_reference(manual STM32H7 ".csi_enable = true, ${q}" "LL_RCC_PLL_SetSource(LL_RCC_PLLSOURCE_CSI);"
    src_csi 4 "25 MHz")
check_reference(manual_m STM32H7 "${q}" "LL_RCC_PLL1_SetM(4U);" src_hsi 4 "400 MHz")
check_reference(l4 STM32L4 ".pll[0] = { .m = 4, .n = 48, .q = 4 }" "" src_hsi 4 "48 MHz")

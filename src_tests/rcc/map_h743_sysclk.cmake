cmake_minimum_required(VERSION 3.29)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(dir "${CMAKE_CURRENT_BINARY_DIR}/rcc_map_h743_sysclk")
file(MAKE_DIRECTORY "${dir}")

function(check_source name hse csi pll_config override source frequency)
    set(input "${dir}/${name}.c")
    file(WRITE "${input}" "
#include <stdbool.h>
#include <stm32h7xx_ll_rcc.h>
#include \"rcc/uni_hal_rcc_stm32h7.h\"
enum { BOARD_N = 100, BOARD_P = 2 };
const uni_hal_rcc_stm32h7_config_t clocks = {
    .hse_enable = ${hse}, .csi_enable = ${csi}, .pll[0] = { ${pll_config} },
};
void configure(void) { ${override} }
")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../../cmake/fun_rcc_map.py"
        --target-name clocks --define STM32H7 --hse-hz 8000000 --input-file "${input}"
        --dot-file "${dir}/${name}.dot" --png-file "${dir}/${name}.png" --svg-file "${dir}/${name}.svg"
        COMMAND_ERROR_IS_FATAL ANY)
    file(READ "${dir}/${name}.dot" graph)
    string(FIND "${graph}" "\"${source}\" -> \"bus_sysclk\"" edge)
    string(FIND "${graph}" "\"bus_sysclk\" [label=\"SYSCLK\\n${frequency}\"]" rate)
    if(edge EQUAL -1 OR rate EQUAL -1)
        message(FATAL_ERROR "${name} must show SYSCLK from ${source} at ${frequency}")
    endif()
endfunction()

set(q_only ".m = 4, .n = 100, .q = 4")
check_source(hse_first true true "${q_only}" "" src_hse "8 MHz")
check_source(csi_only false true "${q_only}" "" src_csi "4 MHz")
check_source(hsi_only false false "${q_only}" "" src_hsi "64 MHz")
check_source(unknown_pll true false ".m = 4, .n = BOARD_N, .p = 2" "" pll1_p "?")
check_source(explicit_csi true true ".m = 4, .n = 100, .p = 2"
    "LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_CSI);" src_csi "4 MHz")
check_source(explicit_pll true false ".m = 4, .n = 100, .p = BOARD_P"
    "LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL1);" pll1_p "?")

cmake_minimum_required(VERSION 3.29)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(dir "${CMAKE_CURRENT_BINARY_DIR}/rcc_map_pll_fraction")
file(MAKE_DIRECTORY "${dir}")
file(WRITE "${dir}/clocks.c" [=[
#include <stdbool.h>
#include "rcc/uni_hal_rcc_stm32h7.h"
const uni_hal_rcc_stm32h7_config_t clocks = {
    .hse_enable = true,
    .pll[0] = { .m = 4, .n = 100, .p = 2, .q = 4, .r = 8, .fracn = 4096 },
    .pll[1] = { .m = 4, .n = 120, .q = 4, .fracn = 0 },
    .pll[2] = { .m = 4, .n = 128, .r = 8 },
};
]=])
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../../cmake/fun_rcc_map.py"
    --target-name clocks --define STM32H7 --hse-hz 8000000 --input-file "${dir}/clocks.c"
    --dot-file "${dir}/clocks.dot" --png-file "${dir}/clocks.png" --svg-file "${dir}/clocks.svg"
    COMMAND_ERROR_IS_FATAL ANY)
file(READ "${dir}/clocks.dot" graph)
foreach(label IN ITEMS [[PLL1\nN=100\nVCO=201 MHz\nFRACN=4096]]
                       [[PLL1P\n100.5 MHz]] [[PLL1Q\n50.25 MHz]] [[PLL1R\n25.125 MHz]]
                       [[SYSCLK\n100.5 MHz]] [[HCLK\n50.25 MHz]] [[PLL2Q\n60 MHz]] [[PLL3R\n32 MHz]])
    string(FIND "${graph}" "label=\"${label}\"" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Clock map is missing '${label}'")
    endif()
endforeach()

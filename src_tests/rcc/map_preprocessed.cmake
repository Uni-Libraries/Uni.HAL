cmake_minimum_required(VERSION 3.29)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(input "${CMAKE_CURRENT_LIST_DIR}/fixtures/${FIXTURE}.c")
set(dir "${CMAKE_CURRENT_BINARY_DIR}/rcc_map_${FIXTURE}")
file(MAKE_DIRECTORY "${dir}")
if(MSVC)
    set(preprocess /nologo /EP)
else()
    set(preprocess -E -P)
endif()
execute_process(COMMAND "${CC}" ${preprocess} "${input}" OUTPUT_FILE "${dir}/active.c"
    COMMAND_ERROR_IS_FATAL ANY)

foreach(mode IN ITEMS source reference)
    if(mode STREQUAL reference)
        set(input "${dir}/active.c")
    endif()
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../../cmake/fun_rcc_map.py"
        --target-name clocks --define STM32H7 --input-file "${input}" --dot-file "${dir}/${mode}.dot"
        --png-file "${dir}/${mode}.png" --svg-file "${dir}/${mode}.svg"
        COMMAND_ERROR_IS_FATAL ANY)
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${dir}/source.dot" "${dir}/reference.dot"
    COMMAND_ERROR_IS_FATAL ANY)

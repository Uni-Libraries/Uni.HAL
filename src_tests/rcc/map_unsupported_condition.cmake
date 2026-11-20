cmake_minimum_required(VERSION 3.29)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(dir "${CMAKE_CURRENT_BINARY_DIR}/rcc_map_unsupported")
file(MAKE_DIRECTORY "${dir}")
file(WRITE "${dir}/unsupported.c" "#if HSE_VALUE >> 2\n#endif\n")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../../cmake/fun_rcc_map.py"
    --target-name clocks --input-file "${dir}/unsupported.c" --dot-file "${dir}/map.dot"
    --png-file "${dir}/map.png" --svg-file "${dir}/map.svg"
    RESULT_VARIABLE status ERROR_VARIABLE diagnostic)
if(status EQUAL 0 OR NOT diagnostic MATCHES "unsupported.c: #if HSE_VALUE >> 2:")
    message(FATAL_ERROR "Expected a failure that identifies the unsupported condition, got ${status}: ${diagnostic}")
endif()

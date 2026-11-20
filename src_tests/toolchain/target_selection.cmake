cmake_minimum_required(VERSION 3.29)
set(dir "${CMAKE_CURRENT_BINARY_DIR}/toolchain_target_selection")
file(MAKE_DIRECTORY "${dir}/source")
file(WRITE "${dir}/source/CMakeLists.txt" [[
cmake_minimum_required(VERSION 3.29)
project(toolchain_consumer LANGUAGES C)
]])

foreach(mcu IN ITEMS "" "STM32H734")
    execute_process(COMMAND "${CMAKE_COMMAND}" --fresh -S "${dir}/source" -B "${dir}/build"
        -G "${GENERATOR}" "-DUNI_HAL_TARGET_MCU=${mcu}"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_LIST_DIR}/../../cmake/toolchain.cmake"
        "-DCMAKE_C_COMPILER=${dir}/missing-compiler"
        RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(rc EQUAL 0 OR NOT error MATCHES "UNI_HAL_TARGET_MCU must be one of")
        message(FATAL_ERROR "Expected an MCU selection error before compiler detection for '${mcu}'\n${output}\n${error}")
    endif()
endforeach()

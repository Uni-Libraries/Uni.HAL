cmake_minimum_required(VERSION 3.29)
set(dir "${CMAKE_CURRENT_BINARY_DIR}/rcc_map_multi_config")

set(repo "${CMAKE_CURRENT_LIST_DIR}/../..")
set(hal "${dir}/deps/hardware-abstraction")
set(app "${dir}/application")
file(MAKE_DIRECTORY "${app}" "${hal}/cmake" "${hal}/src/i2c" "${hal}/src/rcc")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/map_consumer/" DESTINATION "${app}")
file(COPY "${repo}/cmake/fun_rcc_map.cmake" "${repo}/cmake/fun_rcc_map.py" DESTINATION "${hal}/cmake")
file(COPY "${repo}/src/i2c/uni_hal_i2c.h" "${repo}/src/i2c/uni_hal_i2c_stm32h7.c" DESTINATION "${hal}/src/i2c")
file(COPY "${repo}/src/rcc/uni_hal_rcc_stm32h7.c" DESTINATION "${hal}/src/rcc")
set(driver "${hal}/src/rcc/uni_hal_rcc_stm32l4.c")
file(WRITE "${driver}" "void clocks(void) { LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_4); }")

function(configure rev)
    execute_process(COMMAND "${CMAKE_COMMAND}" -S "${app}" -B "${dir}/build" "-DHAL_ROOT=${hal}"
        -G "Ninja Multi-Config" "-DCMAKE_MAKE_PROGRAM=${NINJA}" "-DCMAKE_C_COMPILER=${CC}"
        "-DCMAKE_SYSTEM_NAME=${SYSTEM}" "-DBOARD_REV=${rev}"
        COMMAND_ERROR_IS_FATAL ANY)
endfunction()

function(build_and_check config source divider ahb)
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${dir}/build" --config "${config}" --target firmware
        COMMAND_ERROR_IS_FATAL ANY)
    set(dot "${dir}/build/rcc_map/firmware/${config}/rcc_map.dot")
    file(READ "${dot}" graph)
    string(FIND "${graph}" "\"src_${source}\" -> \"mco1\" [label=\"/${divider}\"" edge)
    string(FIND "${graph}" "\"bus_sysclk\" -> \"bus_hclk\" [label=\"/${ahb}\"" bus_edge)
    if(bus_edge EQUAL -1)
        message(FATAL_ERROR "${dot} is missing the AHB divider /${ahb} from ${driver}")
    endif()
    if(edge EQUAL -1)
        message(FATAL_ERROR "${dot} is missing ${source}/${divider} on MCO1")
    endif()
endfunction()

configure(1)
build_and_check(Debug hsi 4 4)
build_and_check(Release hse 2 4)
build_and_check(Debug hsi 4 4)
configure(2)
build_and_check(Debug lsi 8 4)

# An incremental build must also observe a change to the HAL driver.
file(WRITE "${driver}" "void clocks(void) { LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_8); }")
build_and_check(Debug lsi 8 8)

cmake_minimum_required(VERSION 3.29)
set(dir "${CMAKE_CURRENT_BINARY_DIR}/rcc_map_multi_config")

function(configure rev)
    execute_process(COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}/map_consumer" -B "${dir}"
        -G "Ninja Multi-Config" "-DCMAKE_MAKE_PROGRAM=${NINJA}" "-DCMAKE_C_COMPILER=${CC}"
        "-DCMAKE_SYSTEM_NAME=${SYSTEM}" "-DBOARD_REV=${rev}"
        COMMAND_ERROR_IS_FATAL ANY)
endfunction()

function(build_and_check config source divider)
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${dir}" --config "${config}" --target firmware
        COMMAND_ERROR_IS_FATAL ANY)
    set(dot "${dir}/rcc_map/firmware/${config}/rcc_map.dot")
    file(READ "${dot}" graph)
    string(FIND "${graph}" "\"src_${source}\" -> \"mco1\" [label=\"/${divider}\"" edge)
    if(edge EQUAL -1)
        message(FATAL_ERROR "${dot} is missing ${source}/${divider} on MCO1")
    endif()
endfunction()

configure(1)
build_and_check(Debug hsi 4)
build_and_check(Release hse 2)
build_and_check(Debug hsi 4)
configure(2)
build_and_check(Debug lsi 8)

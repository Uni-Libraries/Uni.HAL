add_subdirectory(${CMAKE_CURRENT_LIST_DIR} "${CMAKE_CURRENT_BINARY_DIR}/uni_hal")



function(uni_hal_set_linker_file executable_name file_name)
    set_target_properties(${executable_name} PROPERTIES LINK_DEPENDS "${file_name}")
    set_target_properties(${executable_name} PROPERTIES LINK_FLAGS   "-T ${file_name}")
endfunction()



function(uni_hal_add_library library_name)
    add_library(${library_name} STATIC)
    target_link_libraries(${library_name} PUBLIC uni.hal)
    target_include_directories(${library_name} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()



function(uni_hal_add_executable executable_name)
    add_executable(${executable_name})

    target_link_libraries(${executable_name} PRIVATE uni.hal)
    target_include_directories(${executable_name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

    # Linker file
    get_property(UNI_HAL_LINKER_FILE GLOBAL PROPERTY UNI_HAL_LINKER_FILE)
    if(DEFINED UNI_HAL_LINKER_FILE)
        uni_hal_set_linker_file(${executable_name} ${UNI_HAL_LINKER_FILE})
    endif()

    # Extension
    set_target_properties(${executable_name} PROPERTIES SUFFIX ".elf")

    # Conversion to binary
    add_custom_command(
        TARGET ${executable_name}
        POST_BUILD
        COMMAND ${CMAKE_OBJCOPY}
        ARGS -Obinary ${executable_name}.elf ${executable_name}.bin
    )
endfunction()



function(uni_hal_add_file project_name in_file out_file out_var)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${in_file}" hex HEX)
    file(SIZE "${CMAKE_CURRENT_SOURCE_DIR}/${in_file}" file_size)
    string(REGEX REPLACE "(................)" "\\1\n" hex ${hex})
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex ${hex})

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${out_file}.h"
        "#pragma once \n"
        "#include <stddef.h> \n"
        "#include <stdint.h> \n"
        "extern const uint8_t ${out_var}[];\n"
        "#define ${out_var}_size (${file_size})\n"
    )

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${out_file}.c"
        "#include \"${out_file}.h\"\n"
        "\n"
        "const uint8_t ${out_var}[] = {\n"
        "${hex}\n"
        "};\n"
    )
    target_sources(${project_name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${out_file}.c")
endfunction()

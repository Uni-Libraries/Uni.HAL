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



function(uni_hal_add_file target_name in_file out_file out_var)
    set(in_file_full "${CMAKE_CURRENT_SOURCE_DIR}/${in_file}")
    set(out_c_file "${CMAKE_CURRENT_BINARY_DIR}/${out_file}.c")
    set(out_h_file "${CMAKE_CURRENT_BINARY_DIR}/${out_file}.h")
    set(script_file "${CMAKE_CURRENT_BINARY_DIR}/uni.hal.fileconvert.cmake")

    set(script_content "
# This script is generated by uni_hal_add_file function in uni.hal.cmake
# Do not edit this file manually.

if(NOT DEFINED INPUT_FILE)
    message(FATAL_ERROR \"INPUT_FILE variable is not defined.\")
endif()
if(NOT EXISTS \"\${INPUT_FILE}\")
    message(FATAL_ERROR \"Input file does not exist: \${INPUT_FILE}\")
endif()
if(NOT DEFINED OUT_C_FILE)
    message(FATAL_ERROR \"OUT_C_FILE variable is not defined.\")
endif()
if(NOT DEFINED OUT_H_FILE)
    message(FATAL_ERROR \"OUT_H_FILE variable is not defined.\")
endif()
if(NOT DEFINED OUT_FILE_REL)
    message(FATAL_ERROR \"OUT_FILE_REL variable is not defined.\")
endif()
if(NOT DEFINED OUT_VAR)
    message(FATAL_ERROR \"OUT_VAR variable is not defined.\")
endif()

file(READ \"\${INPUT_FILE}\" hex_content HEX)
file(SIZE \"\${INPUT_FILE}\" file_size)

string(REGEX REPLACE \"(................)\" \"\\\\1\\\\n\" formatted_hex \"\${hex_content}\")
string(REGEX REPLACE \"([0-9a-f][0-9a-f])\" \"0x\\\\1,\" formatted_hex \"\${formatted_hex}\")

file(WRITE \"\${OUT_H_FILE}\"
    \"#pragma once\\n\"
    \"#include <stddef.h>\\n\"
    \"#include <stdint.h>\\n\"
    \"\\n\"
    \"extern const uint8_t \${OUT_VAR}[];\\n\"
    \"#define \${OUT_VAR}_size (\${file_size})\\n\"
)

file(WRITE \"\${OUT_C_FILE}\"
    \"#include \\\"\${OUT_FILE_REL}.h\\\"\\n\"
    \"\\n\"
    \"const uint8_t \${OUT_VAR}[] = {\\n\"
    \"\${formatted_hex}\\n\"
    \"};\\n\"
)
")

    file(WRITE ${script_file} "${script_content}")

    add_custom_command(
        OUTPUT "${out_c_file}" "${out_h_file}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE=${in_file_full}
            -DOUT_C_FILE=${out_c_file}
            -DOUT_H_FILE=${out_h_file}
            -DOUT_FILE_REL=${out_file}
            -DOUT_VAR=${out_var}
            -P "${script_file}"
        MAIN_DEPENDENCY "${in_file_full}"
        DEPENDS "${in_file_full}" "${script_file}"
        COMMENT "Generating ${out_c_file} and ${out_h_file} from ${in_file}"
        VERBATIM
    )

    target_sources(${target_name} PRIVATE "${out_c_file}")
    target_include_directories(${target_name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

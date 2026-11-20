include_guard(GLOBAL)


#
# Generate RCC clock map artifacts for an existing target.
#
# Usage:
#   uni_hal_generate_rcc_map(<target_name> <file_path> [<file_path> ...])
#
# The map uses the target definitions for the primary input's language.
# Multi-configuration builds put each map in rcc_map/<target>/<config>/.
#
# Minimal example:
#   uni_hal_generate_rcc_map(my_app ${CMAKE_SOURCE_DIR}/src_lib/config/app_config_rcc.c ${CMAKE_SOURCE_DIR}/src_app/app.c)
#
function(uni_hal_generate_rcc_map target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "uni_hal_generate_rcc_map: target does not exist: ${target_name}")
    endif()

    if(NOT ARGN)
        message(FATAL_ERROR "uni_hal_generate_rcc_map: at least one RCC-related input file must be provided")
    endif()

    set(_rcc_input_files "")
    foreach(_in_file IN LISTS ARGN)
        if(IS_ABSOLUTE "${_in_file}")
            set(_in_file_abs "${_in_file}")
        else()
            get_filename_component(_in_file_abs "${_in_file}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        endif()

        if(NOT EXISTS "${_in_file_abs}")
            message(FATAL_ERROR "uni_hal_generate_rcc_map: input file does not exist: ${_in_file_abs}")
        endif()

        list(APPEND _rcc_input_files "${_in_file_abs}")
    endforeach()

    list(GET _rcc_input_files 0 _rcc_input_file)

    set(_rcc_map_out_dir "${CMAKE_CURRENT_BINARY_DIR}/rcc_map/${target_name}")
    get_property(_rcc_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(_rcc_multi_config)
        string(APPEND _rcc_map_out_dir "/$<CONFIG>")
    endif()
    set(_rcc_map_dot_file "${_rcc_map_out_dir}/rcc_map.dot")
    set(_rcc_map_png_file "${_rcc_map_out_dir}/rcc_map.png")
    set(_rcc_map_svg_file "${_rcc_map_out_dir}/rcc_map.svg")
    set(_rcc_map_script   "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/fun_rcc_map.py")

    get_filename_component(_rcc_hal_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(_rcc_hal_inputs
        "${_rcc_hal_root}/src/i2c/uni_hal_i2c.h"
        "${_rcc_hal_root}/src/i2c/uni_hal_i2c_stm32h7.c"
        "${_rcc_hal_root}/src/rcc/uni_hal_rcc_stm32h7.c"
        "${_rcc_hal_root}/src/rcc/uni_hal_rcc_stm32l4.c")

    if(NOT EXISTS "${_rcc_map_script}")
        message(FATAL_ERROR "uni_hal_generate_rcc_map: generator script was not found: ${_rcc_map_script}")
    endif()

    set(_rcc_map_hse_hz "")
    if(DEFINED UNI_HAL_HSE_VALUE)
        set(_rcc_map_hse_hz "${UNI_HAL_HSE_VALUE}")
    elseif(DEFINED HSE_VALUE)
        set(_rcc_map_hse_hz "${HSE_VALUE}")
    endif()

    set(_rcc_map_hsi_hz "")
    if(DEFINED UNI_HAL_HSI_VALUE)
        set(_rcc_map_hsi_hz "${UNI_HAL_HSI_VALUE}")
    endif()

    set(_rcc_map_hsi48_hz "")
    if(DEFINED UNI_HAL_HSI48_VALUE)
        set(_rcc_map_hsi48_hz "${UNI_HAL_HSI48_VALUE}")
    endif()

    set(_rcc_map_csi_hz "")
    if(DEFINED UNI_HAL_CSI_VALUE)
        set(_rcc_map_csi_hz "${UNI_HAL_CSI_VALUE}")
    endif()

    set(_rcc_map_lsi_hz "")
    if(DEFINED UNI_HAL_LSI_VALUE)
        set(_rcc_map_lsi_hz "${UNI_HAL_LSI_VALUE}")
    endif()

    set(_rcc_map_lse_hz "")
    if(DEFINED UNI_HAL_LSE_VALUE)
        set(_rcc_map_lse_hz "${UNI_HAL_LSE_VALUE}")
    endif()

    get_source_file_property(_rcc_language "${_rcc_input_file}" LANGUAGE)
    if(NOT _rcc_language)
        # Headers and inputs owned by another target may have no source property.
        get_property(_rcc_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
        if("C" IN_LIST _rcc_languages)
            set(_rcc_language C)
        else()
            list(GET _rcc_languages 0 _rcc_language)
        endif()
    endif()

    # Evaluate the target's own and transitive definitions at generation time.
    # A dependency on this file also invalidates maps after CMake reconfiguration.
    set(_rcc_definitions_file "${_rcc_map_out_dir}/definitions.rsp")
    file(GENERATE OUTPUT "${_rcc_definitions_file}"
        CONTENT "$<JOIN:$<LIST:TRANSFORM,$<TARGET_PROPERTY:${target_name},COMPILE_DEFINITIONS>,PREPEND,--define=>,\n>"
        CONDITION "$<COMPILE_LANGUAGE:${_rcc_language}>" TARGET "${target_name}")

    if(NOT Python_EXECUTABLE)
        find_package(Python COMPONENTS Interpreter REQUIRED)
    endif()

    set(_rcc_input_args "")
    foreach(_input_file IN LISTS _rcc_input_files)
        list(APPEND _rcc_input_args "--input-file" "${_input_file}")
    endforeach()

    set(_rcc_freq_args "")
    if(NOT _rcc_map_hse_hz STREQUAL "")
        list(APPEND _rcc_freq_args "--hse-hz" "${_rcc_map_hse_hz}")
    endif()
    if(NOT _rcc_map_hsi_hz STREQUAL "")
        list(APPEND _rcc_freq_args "--hsi-hz" "${_rcc_map_hsi_hz}")
    endif()
    if(NOT _rcc_map_hsi48_hz STREQUAL "")
        list(APPEND _rcc_freq_args "--hsi48-hz" "${_rcc_map_hsi48_hz}")
    endif()
    if(NOT _rcc_map_csi_hz STREQUAL "")
        list(APPEND _rcc_freq_args "--csi-hz" "${_rcc_map_csi_hz}")
    endif()
    if(NOT _rcc_map_lsi_hz STREQUAL "")
        list(APPEND _rcc_freq_args "--lsi-hz" "${_rcc_map_lsi_hz}")
    endif()
    if(NOT _rcc_map_lse_hz STREQUAL "")
        list(APPEND _rcc_freq_args "--lse-hz" "${_rcc_map_lse_hz}")
    endif()

    add_custom_command(
        OUTPUT "${_rcc_map_dot_file}"
        BYPRODUCTS "${_rcc_map_png_file}" "${_rcc_map_svg_file}"
        COMMAND "${Python_EXECUTABLE}" "${_rcc_map_script}"
            "--target-name" "${target_name}"
            "--hal-root" "${_rcc_hal_root}"
            "--primary-input" "${_rcc_input_file}"
            ${_rcc_input_args}
            "--dot-file" "${_rcc_map_dot_file}"
            "--png-file" "${_rcc_map_png_file}"
            "--svg-file" "${_rcc_map_svg_file}"
            "@${_rcc_definitions_file}"
            ${_rcc_freq_args}
        # A MAIN_DEPENDENCY would replace the compile rule for that source.
        DEPENDS ${_rcc_input_files} ${_rcc_hal_inputs} "${_rcc_map_script}" "${_rcc_definitions_file}"
        COMMENT "Generating RCC map for target ${target_name}"
        VERBATIM
    )

    set(_rcc_map_target "${target_name}__rcc_map")
    add_custom_target("${_rcc_map_target}" DEPENDS "${_rcc_map_dot_file}")
    add_dependencies("${target_name}" "${_rcc_map_target}")
endfunction()

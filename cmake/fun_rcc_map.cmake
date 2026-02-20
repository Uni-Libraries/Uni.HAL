include_guard(GLOBAL)


#
# Generate RCC clock map artifacts for an existing target.
#
# Usage:
#   uni_hal_generate_rcc_map(<target_name> <file_path> [<file_path> ...])
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
    set(_rcc_map_dot_file "${_rcc_map_out_dir}/rcc_map.dot")
    set(_rcc_map_png_file "${_rcc_map_out_dir}/rcc_map.png")
    set(_rcc_map_svg_file "${_rcc_map_out_dir}/rcc_map.svg")
    set(_rcc_map_script   "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/fun_rcc_map.py")

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

    get_directory_property(_rcc_dir_definitions COMPILE_DEFINITIONS)
    get_target_property(_rcc_target_definitions "${target_name}" COMPILE_DEFINITIONS)
    if(_rcc_target_definitions STREQUAL "NOTFOUND")
        set(_rcc_target_definitions "")
    endif()

    set(_rcc_map_definitions "${_rcc_dir_definitions};${_rcc_target_definitions}")

    if(DEFINED UNI_HAL_TARGET_MCU)
        string(TOUPPER "${UNI_HAL_TARGET_MCU}" _rcc_mcu_upper)
        if(_rcc_mcu_upper MATCHES "^STM32H7")
            list(APPEND _rcc_map_definitions "STM32H7")
        elseif(_rcc_mcu_upper MATCHES "^STM32L4")
            list(APPEND _rcc_map_definitions "STM32L4")
        endif()
    endif()

    if(NOT _rcc_map_hse_hz STREQUAL "")
        list(APPEND _rcc_map_definitions "HSE_VALUE=${_rcc_map_hse_hz}")
    endif()
    if(NOT _rcc_map_hsi_hz STREQUAL "")
        list(APPEND _rcc_map_definitions "HSI_VALUE=${_rcc_map_hsi_hz}")
    endif()
    if(NOT _rcc_map_hsi48_hz STREQUAL "")
        list(APPEND _rcc_map_definitions "HSI48_VALUE=${_rcc_map_hsi48_hz}")
    endif()
    if(NOT _rcc_map_csi_hz STREQUAL "")
        list(APPEND _rcc_map_definitions "CSI_VALUE=${_rcc_map_csi_hz}")
    endif()
    if(NOT _rcc_map_lsi_hz STREQUAL "")
        list(APPEND _rcc_map_definitions "LSI_VALUE=${_rcc_map_lsi_hz}")
    endif()
    if(NOT _rcc_map_lse_hz STREQUAL "")
        list(APPEND _rcc_map_definitions "LSE_VALUE=${_rcc_map_lse_hz}")
    endif()

    if(_rcc_map_definitions)
        list(REMOVE_DUPLICATES _rcc_map_definitions)
    endif()

    if(NOT Python_EXECUTABLE)
        find_package(Python COMPONENTS Interpreter REQUIRED)
    endif()

    set(_rcc_input_args "")
    foreach(_input_file IN LISTS _rcc_input_files)
        list(APPEND _rcc_input_args "--input-file" "${_input_file}")
    endforeach()

    set(_rcc_define_args "")
    foreach(_def IN LISTS _rcc_map_definitions)
        if(NOT _def STREQUAL "")
            list(APPEND _rcc_define_args "--define" "${_def}")
        endif()
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
            "--primary-input" "${_rcc_input_file}"
            ${_rcc_input_args}
            "--dot-file" "${_rcc_map_dot_file}"
            "--png-file" "${_rcc_map_png_file}"
            "--svg-file" "${_rcc_map_svg_file}"
            ${_rcc_define_args}
            ${_rcc_freq_args}
        MAIN_DEPENDENCY "${_rcc_input_file}"
        DEPENDS ${_rcc_input_files} "${_rcc_map_script}"
        COMMENT "Generating RCC map for target ${target_name}"
        VERBATIM
    )

    set(_rcc_map_target "${target_name}__rcc_map")
    add_custom_target("${_rcc_map_target}" DEPENDS "${_rcc_map_dot_file}")
    add_dependencies("${target_name}" "${_rcc_map_target}")
endfunction()

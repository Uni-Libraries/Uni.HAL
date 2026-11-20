enable_testing()
include(Catch)

add_test(NAME toolchain_msvc_compiler_paths
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/msvc_compiler_paths.cmake")
add_test(NAME rcc_map_mco_outputs
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/rcc/map_mco_outputs.cmake")
add_test(NAME rcc_map_inactive_clocks
    COMMAND "${CMAKE_COMMAND}" "-DCC=${CMAKE_C_COMPILER}" "-DMSVC=${MSVC}"
        -DFIXTURE=inactive_clocks -P "${CMAKE_CURRENT_SOURCE_DIR}/rcc/map_preprocessed.cmake")
add_test(NAME rcc_map_unsupported_condition
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/rcc/map_unsupported_condition.cmake")
foreach(hse IN ITEMS 8000000 4000000 2000000)
    add_test(NAME rcc_map_hse_${hse}
        COMMAND "${CMAKE_COMMAND}" "-DCC=${CMAKE_C_COMPILER}" "-DMSVC=${MSVC}"
            -DFIXTURE=hse_conditions "-DHSE=${hse}" -P "${CMAKE_CURRENT_SOURCE_DIR}/rcc/map_preprocessed.cmake")
endforeach()


find_program(UNI_HAL_NINJA ninja)
if(UNI_HAL_NINJA)
    add_test(NAME rcc_map_target_definitions
        COMMAND "${CMAKE_COMMAND}" "-DCC=${CMAKE_C_COMPILER}" "-DNINJA=${UNI_HAL_NINJA}"
            "-DSYSTEM=${CMAKE_SYSTEM_NAME}" -P "${CMAKE_CURRENT_SOURCE_DIR}/rcc/map_target_definitions.cmake")
endif()

find_program(UNI_HAL_PWSH pwsh)
if(UNI_HAL_PWSH)
    add_test(NAME vendor_archive_integrity COMMAND "${UNI_HAL_PWSH}" -NoProfile -File
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/archive.ps1")
    add_test(NAME vendor_update_transaction COMMAND "${UNI_HAL_PWSH}" -NoProfile -File
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/transaction.ps1")
    add_test(NAME vendor_patch_checkout COMMAND "${UNI_HAL_PWSH}" -NoProfile -File
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/patch_checkout.ps1")
endif()

function(uni_hal_add_test test_path)
    string(REPLACE "/" "_" test_name ${test_path})

    add_executable(uni_hal_test_${test_name})

    file(GLOB __files CONFIGURE_DEPENDS
            "${test_path}/*.c"
            "${test_path}/*.cpp"
            "${test_path}/*.h"
    )
    target_sources(uni_hal_test_${test_name} PRIVATE ${__files})

    target_include_directories(uni_hal_test_${test_name} PRIVATE "${test_path}")

    target_link_libraries(uni_hal_test_${test_name} PRIVATE Catch2::Catch2WithMain)
    target_link_libraries(uni_hal_test_${test_name} PRIVATE uni.hal)

    # compile definitions
    target_compile_definitions(uni_hal_test_${test_name} PRIVATE BUILD_TESTS)
    if (MSVC)
        target_compile_definitions(uni_hal_test_${test_name} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    #language standard
    if(MSVC)
        set_target_properties(uni_hal_test_${test_name} PROPERTIES C_STANDARD 17)
    else()
        set_target_properties(uni_hal_test_${test_name} PROPERTIES C_STANDARD 23)
    endif()
    set_target_properties(uni_hal_test_${test_name} PROPERTIES C_STANDARD_REQUIRED ON)

    set_target_properties(uni_hal_test_${test_name} PROPERTIES CXX_STANDARD 23)
    set_target_properties(uni_hal_test_${test_name} PROPERTIES CXX_STANDARD_REQUIRED ON)

    catch_discover_tests(uni_hal_test_${test_name} ${ARGN})
endfunction()



#
# Discover
#

uni_hal_add_test(io PROPERTIES TIMEOUT 10)
uni_hal_add_test(ads1015 PROPERTIES TIMEOUT 10)
uni_hal_add_test(dwt)
uni_hal_add_test(rtc)
add_test(NAME freertos_pc_minimal_task COMMAND "${CMAKE_CTEST_COMMAND}"
    --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/freertos_pc" "${CMAKE_CURRENT_BINARY_DIR}/freertos_pc"
    --build-generator "${CMAKE_GENERATOR}" --build-generator-platform "${CMAKE_GENERATOR_PLATFORM}"
    --build-generator-toolset "${CMAKE_GENERATOR_TOOLSET}" --build-noclean -C "$<CONFIG>"
    --build-options "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}" "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_CROSSCOMPILING_EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}" "-DCMAKE_BUILD_TYPE=$<CONFIG>"
    --test-command "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" --output-on-failure --no-tests=error)
add_subdirectory(rng)
uni_hal_add_test(i2c)
uni_hal_add_test(can)
uni_hal_add_test(crc)
add_subdirectory(flash_read)
add_subdirectory(public_api)
add_subdirectory(i2c_transfer)
add_subdirectory(i2c_h7)
add_subdirectory(i2c_recovery)
add_subdirectory(i2c_timeout)
add_subdirectory(io_sync)
add_subdirectory(uart_zynq)

uni_hal_add_test(rcc PROPERTIES TIMEOUT 10)
target_include_directories(uni_hal_test_rcc SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/st_stm32l4/include_device"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/st_stm32l4/include_hal"
)

foreach(hz IN ITEMS 128 1000 4000)
    set(target systick_zynq_delay_${hz})
    add_executable(${target} systick/zynq_delay.c)
    target_compile_definitions(${target} PRIVATE TEST_TICK_HZ=${hz})
    target_include_directories(${target} PRIVATE uart_zynq/fakes)
    target_link_libraries(${target} PRIVATE uni.hal)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 10)
endforeach()

add_executable(systick_zynq_time systick/zynq_time.c)
target_include_directories(systick_zynq_time PRIVATE uart_zynq/fakes)
target_link_libraries(systick_zynq_time PRIVATE uni.hal)
add_test(NAME systick_zynq_time COMMAND systick_zynq_time)

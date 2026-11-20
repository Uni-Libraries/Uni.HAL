enable_testing()
include(Catch)

add_test(NAME toolchain_msvc_compiler_paths
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/msvc_compiler_paths.cmake")
add_test(NAME rcc_map_mco_outputs
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/rcc/map_mco_outputs.cmake")


find_program(UNI_HAL_PWSH pwsh)
if(UNI_HAL_PWSH)
    add_test(NAME vendor_archive_integrity COMMAND "${UNI_HAL_PWSH}" -NoProfile -File
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/archive.ps1")
    add_test(NAME vendor_update_transaction COMMAND "${UNI_HAL_PWSH}" -NoProfile -File
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/transaction.ps1")
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
uni_hal_add_test(i2c)
uni_hal_add_test(can)
add_subdirectory(flash_read)
add_subdirectory(public_api)
add_subdirectory(i2c_transfer)
add_subdirectory(i2c_h7)
add_subdirectory(i2c_timeout)
add_subdirectory(io_sync)
add_subdirectory(uart_zynq)

uni_hal_add_test(rcc PROPERTIES TIMEOUT 10)
target_include_directories(uni_hal_test_rcc SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/st_stm32l4/include_device"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/st_stm32l4/include_hal"
)

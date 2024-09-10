enable_testing()
include(Catch)


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

    catch_discover_tests(uni_hal_test_${test_name})
endfunction()



#
# Discover
#

uni_hal_add_test(common/ringbuffer)
uni_hal_add_test(io)

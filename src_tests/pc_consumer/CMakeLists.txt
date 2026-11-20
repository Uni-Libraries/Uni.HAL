cmake_minimum_required(VERSION 3.29)
project(pc_consumer LANGUAGES C CXX ASM)
include(CTest)
add_subdirectory(../.. uni.hal)

if(NOT TARGET uni.hal OR TARGET Catch2::Catch2 OR TARGET uni_hal_test_io)
    message(FATAL_ERROR "BUILD_TESTING=OFF must provide Uni.HAL without its test dependencies")
endif()

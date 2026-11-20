cmake_minimum_required(VERSION 3.29)
project(firmware_app LANGUAGES C CXX ASM)

add_subdirectory(Uni.HAL)
include(Uni.HAL/uni.hal.cmake)
uni_hal_add_executable(firmware)
target_sources(firmware PRIVATE main.c)

#
# Project
#

cmake_minimum_required(VERSION 3.29)
project(uni.hal LANGUAGES C CXX ASM)



#
# Options
#

option(UNI_HAL_TARGET_MCU    "Target MCU" "")
option(UNI_HAL_HSE_VALUE     ""           0 )
option(UNI_HAL_RTOS_HEAP_APP ""           1 )



#
# Dependencies
#

include(cmake/cpm/cpm.cmake)
include(cmake/cpm/cpm-uni.common.cmake)
include(cmake/cpm/cpm-nanoprintf.cmake)
if(UNI_HAL_TARGET_MCU STREQUAL "PC")
    include(cmake/cpm/cpm-catch2.cmake)
endif()



#
# Global definitions
#

add_compile_definitions(UNI_HAL_TARGET_MCU_${UNI_HAL_TARGET_MCU})



#
# ARM CMSIS Core
#

add_subdirectory(3rdparty/arm_cmsis_core)



#
# Segger
#

if(NOT UNI_HAL_TARGET_MCU STREQUAL "PC")
    add_subdirectory(3rdparty/segger_rtt)
    add_subdirectory(3rdparty/segger_systemview)
endif()



#
# FreeRTOS
#

add_subdirectory(3rdparty/freertos_kernel)
add_subdirectory(3rdparty/freertos_tcp)



#
# DFP
#

if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    add_subdirectory(3rdparty/st_stm32h7)
    add_subdirectory(3rdparty/st_usb_device)
    set(UNI_HAL_DFP_LIBRARY st_stm32h7)
    set(UNI_HAL_DFP_SUFFIX  "*_cm.c" "*_cm7.c" "*_stm32.c" "*_stm32h7.c")
elseif(UNI_HAL_TARGET_MCU STREQUAL "STM32L496")
    add_subdirectory(3rdparty/st_stm32l4)
    set(UNI_HAL_DFP_LIBRARY st_stm32l4)
    set(UNI_HAL_DFP_SUFFIX  "*_cm.c" "*_cm4.c" "*_stm32.c" "*_stm32l4.c")
elseif(UNI_HAL_TARGET_MCU STREQUAL "PC")
    set(UNI_HAL_DFP_SUFFIX  "*_pc.c")
else()
    message(FATAL_ERROR "Unknown target MCU. Please set `UNI_HAL_TARGET_MCU` variable")
endif()



#
# Uni.HAL
#

add_subdirectory(src)

if(UNI_HAL_TARGET_MCU STREQUAL "PC")
    add_subdirectory(src_tests)
endif()

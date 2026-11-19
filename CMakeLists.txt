#
# Project
#

cmake_minimum_required(VERSION 3.29)
project(uni.hal LANGUAGES C CXX ASM)



#
# Options
#

set(UNI_HAL_TARGET_MCU "" CACHE STRING "Target MCU: STM32H743, STM32L496, ZYNQ7000 or PC")
set_property(CACHE UNI_HAL_TARGET_MCU PROPERTY STRINGS STM32H743 STM32L496 ZYNQ7000 PC)
option(UNI_HAL_USE_FREERTOS     "build the drivers on top of FreeRTOS; OFF for a main loop without an RTOS" ON)
option(UNI_HAL_CAN_USE_FREERTOS "use FreeRTOS for CAN driver" ON)
option(UNI_HAL_CAN_USE_FD       "CAN messages carry up to 64 data bytes (CAN FD); OFF keeps them at 8" OFF)
option(UNI_HAL_I2C_USE_FREERTOS "use FreeRTOS for I2C driver" ON)
option(UNI_HAL_IO_PRINTF_LARGE_FORMAT "uni_hal_io_stdio_*printf: support ll/j/z/t (%llu, %zu) at the cost of code size" OFF)
if(NOT UNI_HAL_USE_FREERTOS)
    # nothing to build the per-driver RTOS support on
    set(UNI_HAL_CAN_USE_FREERTOS OFF)
    set(UNI_HAL_I2C_USE_FREERTOS OFF)
endif()
if(NOT DEFINED UNI_HAL_HSE_VALUE)
    set(UNI_HAL_HSE_VALUE "" CACHE STRING "HSE clock value in Hz")
endif()



#
# Checks
#

# The clock drivers derive the PLL dividers and every peripheral clock from HSE_VALUE,
# so there is no usable default: a wrong value gives wrong baud rates without any error.
# ZYNQ7000: the clocks are set up by the PS7 configuration (ps7_init/FSBL), not by Uni.HAL.
if(NOT UNI_HAL_TARGET_MCU STREQUAL "PC" AND NOT UNI_HAL_TARGET_MCU STREQUAL "ZYNQ7000"
   AND NOT UNI_HAL_HSE_VALUE MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "UNI_HAL_HSE_VALUE is not set. Please set it to the HSE frequency of the board in Hz, e.g. -DUNI_HAL_HSE_VALUE=8000000")
endif()



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

    #TODO: make configurable
    #add_subdirectory(3rdparty/segger_systemview)
endif()



#
# FreeRTOS
#

if(UNI_HAL_USE_FREERTOS)
    add_subdirectory(3rdparty/freertos_kernel)
endif()



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
elseif(UNI_HAL_TARGET_MCU STREQUAL "ZYNQ7000")
    add_subdirectory(3rdparty/xilinx_zynq7000)
    set(UNI_HAL_DFP_LIBRARY xilinx_zynq7000)
    set(UNI_HAL_DFP_SUFFIX  "*_ca9.c" "*_ca9.S" "*_zynq7000.c")
elseif(UNI_HAL_TARGET_MCU STREQUAL "PC")
    set(UNI_HAL_DFP_SUFFIX  "*_pc.c")
else()
    message(FATAL_ERROR "Unknown target MCU ${UNI_HAL_TARGET_MCU}. Please set `UNI_HAL_TARGET_MCU` variable")
endif()



if(UNI_HAL_TARGET_MCU MATCHES "^STM32")
    include(CheckSymbolExists)
    check_symbol_exists(PICOLIBC_TLS "picolibc.h" UNI_HAL_STM32_PICOLIBC_TLS)
    if(UNI_HAL_STM32_PICOLIBC_TLS)
        target_sources(${UNI_HAL_DFP_LIBRARY} PRIVATE "3rdparty/st_stm32_common/src_extra/picolibc_startup.c")
        target_link_options(${UNI_HAL_DFP_LIBRARY} INTERFACE "LINKER:--wrap=__libc_init_array")
        # Our static image keeps constructors in flash and TLS in RAM, with no runtime loader.
        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
            target_link_options(${UNI_HAL_DFP_LIBRARY} INTERFACE "LINKER:-z,norelro")
        endif()
    endif()
endif()

#
# Uni.HAL
#

add_subdirectory(src)

if(UNI_HAL_TARGET_MCU STREQUAL "PC")
    # must be called in the top-level directory for ctest to find the tests from the build root
    enable_testing()
    add_subdirectory(src_tests)
endif()

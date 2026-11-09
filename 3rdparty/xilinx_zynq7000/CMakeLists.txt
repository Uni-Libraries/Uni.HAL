#
# Xilinx Zynq-7000 (ps7_cortexa9_0) device support: a standalone BSP built by Vitis.
#
# The BSP is not vendored: it is generated from the hardware design (XSA) by the Vitis
# platform flow and differs from board to board. Point UNI_HAL_ZYNQ7000_BSP_DIR at the
# domain directory of the exported platform, e.g.
#   <workspace>/<platform>/export/<platform>/sw/<domain>   (contains include/, lib/, Xilinx.spec)
#

set(UNI_HAL_ZYNQ7000_BSP_DIR "" CACHE PATH "Xilinx standalone BSP for ps7_cortexa9_0 (include/, lib/, Xilinx.spec)")

if(NOT EXISTS "${UNI_HAL_ZYNQ7000_BSP_DIR}/include/xparameters.h")
    message(FATAL_ERROR "UNI_HAL_ZYNQ7000_BSP_DIR='${UNI_HAL_ZYNQ7000_BSP_DIR}' is not a Xilinx standalone BSP (no include/xparameters.h)")
endif()



#
# Library
#

add_library(xilinx_zynq7000 INTERFACE)

# BSP headers are third-party code: SYSTEM keeps their warnings out of -Werror
target_include_directories(xilinx_zynq7000 SYSTEM INTERFACE "${UNI_HAL_ZYNQ7000_BSP_DIR}/include")

# The BSP is built with the system device tree (SDT) flow
target_compile_definitions(xilinx_zynq7000 INTERFACE SDT)

# Every library the BSP provides (libxil, libxilstandalone, libxiltimer, ...), in a group:
# they reference each other and the C library
file(GLOB _bsp_libs "${UNI_HAL_ZYNQ7000_BSP_DIR}/lib/lib*.a")
set(_bsp_group "")
foreach(_lib IN LISTS _bsp_libs)
    get_filename_component(_name "${_lib}" NAME_WE)
    string(REGEX REPLACE "^lib" "" _name "${_name}")
    list(APPEND _bsp_group "-l${_name}")
endforeach()
list(JOIN _bsp_group "," _bsp_group)
target_link_directories(xilinx_zynq7000 INTERFACE "${UNI_HAL_ZYNQ7000_BSP_DIR}/lib")
target_link_options(xilinx_zynq7000 INTERFACE "-specs=${UNI_HAL_ZYNQ7000_BSP_DIR}/Xilinx.spec")
target_link_libraries(xilinx_zynq7000 INTERFACE "-Wl,--start-group,${_bsp_group},-lgcc,-lc,-lm,--end-group")



#
# Linker file
#

# DDR is mapped from 0x0, the first 1 MiB (OCM/boot) is not used by the application
file(STRINGS "${UNI_HAL_ZYNQ7000_BSP_DIR}/include/xparameters.h" _ddr_high REGEX "#define XPAR_PS7_DDR_0_HIGHADDRESS ")
if(NOT _ddr_high MATCHES "0x[0-9a-fA-F]+")
    message(FATAL_ERROR "XPAR_PS7_DDR_0_HIGHADDRESS not found in the BSP xparameters.h")
endif()
math(EXPR UNI_HAL_ZYNQ7000_DDR_LENGTH "${CMAKE_MATCH_0} + 1 - 0x100000" OUTPUT_FORMAT HEXADECIMAL)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/zynq7000_ddr.ld.in" "${CMAKE_CURRENT_BINARY_DIR}/zynq7000_ddr.ld" @ONLY)
set_property(GLOBAL PROPERTY UNI_HAL_LINKER_FILE "${CMAKE_CURRENT_BINARY_DIR}/zynq7000_ddr.ld")
message(STATUS "Uni.HAL ZYNQ7000: BSP ${UNI_HAL_ZYNQ7000_BSP_DIR}, DDR length ${UNI_HAL_ZYNQ7000_DDR_LENGTH}")

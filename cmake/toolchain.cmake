#
# Includes
#

include("${CMAKE_CURRENT_LIST_DIR}/toolchain_machine.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/toolchain_compiler.cmake")

#
# CMake Flags
#

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
set(CMAKE_C_COMPILER_WORKS 1)

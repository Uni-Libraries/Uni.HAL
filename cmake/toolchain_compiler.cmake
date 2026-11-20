# Compiler IDs are not available on the first toolchain read. On later reads
# CMake can supply the full path to cl or cl.exe for either language.
set(_uni_hal_cl_driver [[(^|[/\\])[Cc][Ll](\.[Ee][Xx][Ee])?$]])
if(CMAKE_C_COMPILER MATCHES "${_uni_hal_cl_driver}" OR CMAKE_CXX_COMPILER MATCHES "${_uni_hal_cl_driver}")
    include("${CMAKE_CURRENT_LIST_DIR}/toolchain_compiler_msvc.cmake")
else()
    include("${CMAKE_CURRENT_LIST_DIR}/toolchain_compiler_gcc.cmake")
endif()

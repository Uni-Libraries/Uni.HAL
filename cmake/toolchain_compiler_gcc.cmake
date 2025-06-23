#
# Checks
#

if((TOOLCHAIN_CPU MATCHES "cortex-m") AND (NOT DEFINED CMAKE_C_COMPILER))
    message(STATUS "Cortex-M target detected and C compiler is not set. Setting compiler to arm-none-eabi-gcc.")
    set(CMAKE_C_COMPILER "arm-none-eabi-gcc" CACHE STRING "C compiler" FORCE)
endif()

if((TOOLCHAIN_CPU MATCHES "cortex-m") AND (NOT DEFINED CMAKE_CXX_COMPILER))
    message(STATUS "Cortex-M target detected and CXX compiler is not set. Setting compiler to arm-none-eabi-g++.")
    set(CMAKE_CXX_COMPILER "arm-none-eabi-g++" CACHE STRING "C++ compiler" FORCE)
endif()

if(NOT DEFINED TOOLCHAIN_ARCH)
    message(FATAL_ERROR "TOOLCHAIN_ARCH was not found. You should include compiler component after the device in the toolchain file")
endif()



# common compiler flags
set(COMMON_FLAGS         "${TOOLCHAIN_ARCH}")                          #toolchain flags
set(COMMON_FLAGS         "${COMMON_FLAGS} -Wall -Wextra -Werror -Wpedantic -Wno-psabi") #warnings
set(COMMON_FLAGS         "${COMMON_FLAGS} -fstack-usage")                              #stack usage analysis
set(COMMON_FLAGS         "${COMMON_FLAGS} -ffunction-sections")                        #generate function sections in ELF file
set(COMMON_FLAGS         "${COMMON_FLAGS} -fdata-sections")                            #generate data sections in ELF file
set(COMMON_FLAGS         "${COMMON_FLAGS} -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.")   #strip filepath from __FILE__ macro
set(COMMON_FLAGS         "${COMMON_FLAGS} -fno-omit-frame-pointer")                    #No omitting of frame pointer, helps debugger a lot

#debug flags
if((CMAKE_C_COMPILER MATCHES "gcc") AND (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
    set(COMMON_FLAGS     "${COMMON_FLAGS} -fvar-tracking")                             #track variables, useful for debugging
    set(COMMON_FLAGS     "${COMMON_FLAGS} -fvar-tracking-assignments")                 #track variable assignments, useful for debugging
endif()

# experimental options
#set(COMMON_FLAGS         "${COMMON_FLAGS} -flto -ffat-lto-objects") #LTO

# debug information and optimization flags
set(COMMON_FLAGS_DEBUG          "-O0 -DDEBUG  -ggdb3 -gdwarf-4")
set(COMMON_FLAGS_RELEASE        "-O2 -DNDEBUG")
set(COMMON_FLAGS_RELWITHDEBINFO "-O1 -DNDEBUG -ggdb3 -gdwarf-4")
set(COMMON_FLAGS_MINSIZEREL     "-Os -DNDEBUG")

# assembler flags
set(CMAKE_ASM_FLAGS                "${COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS                "${CMAKE_ASM_FLAGS} -x assembler-with-cpp") #enable assembler<->c++ compatibility
set(CMAKE_ASM_FLAGS_DEBUG          "${COMMON_FLAGS_DEBUG}")
set(CMAKE_ASM_FLAGS_RELEASE        "${COMMON_FLAGS_RELEASE}")
set(CMAKE_ASM_FLAGS_RELWITHDEBINFO "${COMMON_FLAGS_RELWITHDEBINFO}")
set(CMAKE_ASM_FLAGS_MINSIZEREL     "${COMMON_FLAGS_MINSIZEREL}")

# c flags
set(CMAKE_C_FLAGS                "${COMMON_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG          "${COMMON_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_RELEASE        "${COMMON_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${COMMON_FLAGS_RELWITHDEBINFO}")
set(CMAKE_C_FLAGS_MINSIZEREL     "${COMMON_FLAGS_MINSIZEREL}")

# c++ flags
set(CMAKE_CXX_FLAGS                "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS                "${CMAKE_CXX_FLAGS} -fno-sized-deallocation") #disable sized allocation
set(CMAKE_CXX_FLAGS_DEBUG          "${COMMON_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE        "${COMMON_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${COMMON_FLAGS_RELWITHDEBINFO}")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "${COMMON_FLAGS_MINSIZEREL}")


#
# Linker
#

set(CMAKE_EXE_LINKER_FLAGS_INIT "${TOOLCHAIN_ARCH}")                                        #set architecture flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,-gc-sections")          #perform garbage collection
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,-print-memory-usage")   #print memory usage

if(NOT UNI_HAL_TARGET_MCU STREQUAL "PC")
    if(CMAKE_C_COMPILER MATCHES "gcc")
        set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -specs=nano.specs")         #use newlib-nano
        set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -specs=nosys.specs")        #use nosys
    endif()
endif()

#
# CMake
#

SET(CMAKE_SYSTEM arm-cortex-m4 )
SET(CMAKE_SYSTEM_NAME Generic )
SET(CMAKE_SYSTEM_PROCESSOR cortex-m4 )

#
# Architecture flags
#

if(CMAKE_C_COMPILER MATCHES "clang")
    set(TOOLCHAIN_TARGET    "--target=arm-none-eabi")
endif()
set(TOOLCHAIN_CPU       "-mcpu=cortex-m4")   #CPU architecture
set(TOOLCHAIN_FPU       "-mfpu=fpv4-sp-d16") #FPU type
set(TOOLCHAIN_FLOAT_ABI "-mfloat-abi=hard")  #ABI hardness
set(TOOLCHAIN_ISA       "-mthumb")           #Instruction set architecture
set(TOOLCHAIN_FCALLING  "-mlong-calls")      #Function calls mechanism (long-calls provides >64 MiB address space difference)
set(TOOLCHAIN_ENDIAN    "-mlittle-endian")   #Arch endian
set(TOOLCHAIN_ARCH      "${TOOLCHAIN_TARGET} ${TOOLCHAIN_CPU} ${TOOLCHAIN_FPU} ${TOOLCHAIN_FLOAT_ABI} ${TOOLCHAIN_ISA} ${TOOLCHAIN_FCALLING} ${TOOLCHAIN_ENDIAN}")

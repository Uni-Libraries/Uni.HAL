#
# CMake
#

SET(CMAKE_SYSTEM arm-cortex-a9 )
SET(CMAKE_SYSTEM_NAME Generic )
SET(CMAKE_SYSTEM_PROCESSOR cortex-a9 )

#
# Architecture flags
#
# ARM (not Thumb) instruction set: the Xilinx standalone BSP and the FreeRTOS GCC/ARM_CA9
# port are built for it. VFPv3 with 32 double registers, hard-float ABI - the same as the
# BSP libraries (libxil.a), otherwise they do not link.
#

if(CMAKE_C_COMPILER MATCHES "clang")
    set(TOOLCHAIN_TARGET    "--target=arm-none-eabi")
endif()
set(TOOLCHAIN_CPU       "-mcpu=cortex-a9")   #CPU architecture
set(TOOLCHAIN_FPU       "-mfpu=vfpv3")       #FPU type
set(TOOLCHAIN_FLOAT_ABI "-mfloat-abi=hard")  #ABI hardness
set(TOOLCHAIN_ENDIAN    "-mlittle-endian")   #Arch endian
set(TOOLCHAIN_ARCH      "${TOOLCHAIN_TARGET} ${TOOLCHAIN_CPU} ${TOOLCHAIN_FPU} ${TOOLCHAIN_FLOAT_ABI} ${TOOLCHAIN_ENDIAN}")

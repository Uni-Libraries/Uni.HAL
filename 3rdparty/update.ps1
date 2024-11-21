#!/usr/bin/env pwsh

#
# Functions
#

function Get-CMSIS-Core(){
    Invoke-WebRequest -Uri "https://codeload.github.com/ARM-software/CMSIS_5/zip/refs/heads/develop" -OutFile "./~temp/cmsis.zip"
    Expand-Archive -Path "./~temp/cmsis.zip" -DestinationPath "./~temp/cmsis/"
    Remove-Item -Path "./arm_cmsis_core/include/" -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./arm_cmsis_core/License.txt" -ErrorAction SilentlyContinue
    Copy-Item -Path "./~temp/cmsis/CMSIS_5-develop/CMSIS/Core/Include/" -Destination "./arm_cmsis_core/include/" -Recurse
    Copy-Item -Path "./~temp/cmsis/CMSIS_5-develop/LICENSE.txt" -Destination "./arm_cmsis_core/License.txt"
}


function Get-DFP-STM32L4(){
    Remove-Item -Path "./st_stm32l4/include_device"    -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_stm32l4/include_hal"       -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_stm32l4/include_hal/Legacy"-Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_stm32l4/src_device"        -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_stm32l4/src_hal"           -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_stm32l4/src_hal/Legacy"    -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_stm32l4/License.md"     -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_stm32l4/LICENSE.md"     -ErrorAction SilentlyContinue
    
    Invoke-WebRequest -Uri "https://codeload.github.com/STMicroelectronics/cmsis_device_l4/zip/refs/heads/master" -OutFile "./~temp/cmsis_device_l4.zip"
    Invoke-WebRequest -Uri "https://codeload.github.com/STMicroelectronics/stm32l4xx_hal_driver/zip/refs/heads/master" -OutFile "./~temp/stm32l4xx_hal_driver.zip"
    
    Expand-Archive -Path "./~temp/cmsis_device_l4.zip" -DestinationPath "./~temp/"
    Expand-Archive -Path "./~temp/stm32l4xx_hal_driver.zip" -DestinationPath "./~temp/"
    
    Copy-Item -Path "./~temp/cmsis-device-l4-master/Include/"   -Destination "./st_stm32l4/include_device/" -Recurse
    Copy-Item -Path "./~temp/cmsis-device-l4-master/Source/Templates/gcc/" -Destination "./st_stm32l4/src_device/" -Recurse
    Copy-Item -Path "./~temp/cmsis-device-l4-master/Source/Templates/system_stm32l4xx.c" -Destination "./st_stm32l4/src_device/system_stm32l4xx.c"

    Copy-Item -Path "./~temp/stm32l4xx-hal-driver-master/Inc/"        -Destination "./st_stm32l4/include_hal/"        -Recurse
    Copy-Item -Path "./~temp/stm32l4xx-hal-driver-master/Src/"        -Destination "./st_stm32l4/src_hal/"            -Recurse
    Copy-Item -Path "./~temp/stm32l4xx-hal-driver-master/LICENSE.md"   -Destination "./st_stm32l4/" -Recurse
}


function Get-DFP-STM32H7(){
    Remove-Item -Path "./st_stm32h7/include_device" -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_stm32h7/include_hal"    -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_stm32h7/src_device"     -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_stm32h7/src_hal"        -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_stm32h7/License.md" -ErrorAction SilentlyContinue

    Invoke-WebRequest -Uri "https://codeload.github.com/STMicroelectronics/cmsis_device_h7/zip/refs/heads/master"      -OutFile "./~temp/cmsis_device_h7.zip"
    Invoke-WebRequest -Uri "https://codeload.github.com/STMicroelectronics/stm32h7xx_hal_driver/zip/refs/heads/master" -OutFile "./~temp/stm32h7xx_hal_driver.zip"
    
    Expand-Archive -Path "./~temp/cmsis_device_h7.zip" -DestinationPath "./~temp/"
    Expand-Archive -Path "./~temp/stm32h7xx_hal_driver.zip" -DestinationPath "./~temp/"
    
    Copy-Item -Path "./~temp/cmsis-device-h7-master/Include/"                            -Destination "./st_stm32h7/include_device/" -Recurse
    Copy-Item -Path "./~temp/stm32h7xx-hal-driver-master/Inc/"                           -Destination "./st_stm32h7/include_hal/" -Recurse
    Copy-Item -Path "./~temp/cmsis-device-h7-master/Source/Templates/gcc/"               -Destination "./st_stm32h7/src_device/" -Recurse
    Copy-Item -Path "./~temp/cmsis-device-h7-master/Source/Templates/system_stm32h7xx.c" -Destination "./st_stm32h7/src_device/system_stm32h7xx.c"
    Copy-Item -Path "./~temp/stm32h7xx-hal-driver-master/Src/"                           -Destination "./st_stm32h7/src_hal/" -Recurse
    Copy-Item -Path "./~temp/stm32h7xx-hal-driver-master/LICENSE.md"                     -Destination "./st_stm32h7/" -Recurse


}

function Get-USB-STM-Device() {
    Remove-Item -Path "./st_usb_device/include_core" -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_usb_device/include_cdc"  -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_usb_device/src_core"     -Recurse -ErrorAction SilentlyContinue
    Remove-Item -Path "./st_usb_device/src_cdc"      -Recurse -ErrorAction SilentlyContinue

    Remove-Item -Path "./st_usb_device/License.md"  -ErrorAction SilentlyContinue

    Invoke-WebRequest -Uri "https://github.com/STMicroelectronics/stm32_mw_usb_device/archive/refs/heads/master.zip" -OutFile "./~temp/stm32_mw_usb_device.zip"

    Expand-Archive -Path "./~temp/stm32_mw_usb_device.zip" -DestinationPath "./~temp/"

    Copy-Item -Path "./~temp/stm32-mw-usb-device-master/Core/Inc/"      -Destination "./st_usb_device/include_core/" -Recurse
    Copy-Item -Path "./~temp/stm32-mw-usb-device-master/Core/Src/"      -Destination "./st_usb_device/src_core/"     -Recurse

    Copy-Item -Path "./~temp/stm32-mw-usb-device-master/Class/CDC/Inc/" -Destination "./st_usb_device/include_cdc/"  -Recurse
    Copy-Item -Path "./~temp/stm32-mw-usb-device-master/Class/CDC/Src/" -Destination "./st_usb_device/src_cdc/"      -Recurse

    Copy-Item   -Path "./~temp/stm32-mw-usb-device-master/LICENSE.md" -Destination "./st_usb_device"
}


function Get-FreeRTOS-Kernel(){
    Invoke-WebRequest -Uri "https://codeload.github.com/FreeRTOS/FreeRTOS-Kernel/zip/refs/heads/main"      -OutFile "./~temp/freertos_kernel.zip"
    Expand-Archive -Path "./~temp/freertos_kernel.zip" -DestinationPath "./~temp/"

    Remove-Item -Path "./freertos_kernel/src"      -Recurse -ErrorAction SilentlyContinue
    New-Item -Path "./freertos_kernel/src/" -ItemType Directory
    Copy-Item   -Path "./~temp/FreeRTOS-Kernel-main/*.c" -Destination "./freertos_kernel/src/" -Recurse
    
    Remove-Item -Path "./freertos_kernel/include"      -Recurse -ErrorAction SilentlyContinue
    Copy-Item   -Path "./~temp/FreeRTOS-Kernel-main/include" -Destination "./freertos_kernel/" -Recurse
    Remove-Item -Path "./freertos_kernel/include/CMakeLists.txt" -ErrorAction SilentlyContinue
    Remove-Item -Path "./freertos_kernel/include/stdint.readme" -ErrorAction SilentlyContinue

    Remove-Item -Path "./freertos_kernel/src_memmng"      -Recurse -ErrorAction SilentlyContinue
    New-Item    -Path "./freertos_kernel/src_memmng/" -ItemType Directory
    Copy-Item   -Path "./~temp/FreeRTOS-Kernel-main/portable/MemMang/*.c" -Destination "./freertos_kernel/src_memmng" -Recurse

    Remove-Item -Path "./freertos_kernel/src_port"      -Recurse -ErrorAction SilentlyContinue

    New-Item    -Path "./freertos_kernel/src_port/arm_cm4f/" -ItemType Directory
    Copy-Item   -Path "./~temp/FreeRTOS-Kernel-main/portable/GCC/ARM_CM4F/*.c" -Destination "./freertos_kernel/src_port/arm_cm4f/" -Recurse
    Copy-Item   -Path "./~temp/FreeRTOS-Kernel-main/portable/GCC/ARM_CM4F/*.h" -Destination "./freertos_kernel/src_port/arm_cm4f/" -Recurse

    New-Item    -Path "./freertos_kernel/src_port/pc_posix/" -ItemType Directory
    Copy-Item   -Path "./~temp/FreeRTOS-Kernel-main/portable/ThirdParty/GCC/Posix/*.c" -Destination "./freertos_kernel/src_port/pc_posix/" -Recurse
    Copy-Item   -Path "./~temp/FreeRTOS-Kernel-main/portable/ThirdParty/GCC/Posix/*.h" -Destination "./freertos_kernel/src_port/pc_posix/" -Recurse
    New-Item    -Path "./freertos_kernel/src_port/pc_posix/utils/" -ItemType Directory
    Copy-Item   -Path "./~temp/FreeRTOS-Kernel-main/portable/ThirdParty/GCC/Posix/utils/*.c" -Destination "./freertos_kernel/src_port/pc_posix/utils/" -Recurse
    Copy-Item   -Path "./~temp/FreeRTOS-Kernel-main/portable/ThirdParty/GCC/Posix/utils/*.h" -Destination "./freertos_kernel/src_port/pc_posix/utils/" -Recurse

    Copy-Item   -Path "./~temp/FreeRTOS-Kernel-main/LICENSE.md" -Destination "./freertos_kernel"
}


function Get-FreeRTOS-TCP(){
    Invoke-WebRequest -Uri "https://codeload.github.com/FreeRTOS/FreeRTOS-Plus-TCP/zip/refs/heads/main"      -OutFile "./~temp/freertos_tcp.zip"
    Expand-Archive -Path "./~temp/freertos_tcp.zip" -DestinationPath "./~temp/"

    Remove-Item -Path "./freertos_tcp/src"                        -Recurse -ErrorAction SilentlyContinue
    New-Item    -Path "./freertos_tcp/src/"                       -ItemType Directory
    Copy-Item   -Path "./~temp/FreeRTOS-Plus-TCP-main/source/*.c" -Destination "./freertos_tcp/src/" -Recurse
  
    Remove-Item -Path "./freertos_tcp/include"                            -Recurse -ErrorAction SilentlyContinue
    New-Item    -Path "./freertos_tcp/include/"                           -ItemType Directory
    Copy-Item   -Path "./~temp/FreeRTOS-Plus-TCP-main/source/include/*.h" -Destination "./freertos_tcp/include" -Recurse

    Remove-Item -Path "./freertos_tcp/include_gcc"                            -Recurse -ErrorAction SilentlyContinue
    New-Item    -Path "./freertos_tcp/include_gcc/"                           -ItemType Directory
    Copy-Item   -Path "./~temp/FreeRTOS-Plus-TCP-main/source/portable/Compiler/GCC/*.h" -Destination "./freertos_tcp/include_gcc" -Recurse

    Remove-Item -Path "./freertos_tcp/src_buffer"                            -Recurse -ErrorAction SilentlyContinue
    New-Item    -Path "./freertos_tcp/src_buffer/"                           -ItemType Directory
    Copy-Item   -Path "./~temp/FreeRTOS-Plus-TCP-main/source/portable/BufferManagement/*.c" -Destination "./freertos_tcp/src_buffer" -Recurse

    Remove-Item -Path "./freertos_tcp/src_driver/linux"                            -Recurse -ErrorAction SilentlyContinue
    New-Item    -Path "./freertos_tcp/src_driver/linux/"                           -ItemType Directory -ErrorAction SilentlyContinue
    Copy-Item   -Path "./~temp/FreeRTOS-Plus-TCP-main/source/portable/NetworkInterface/linux/*.c" -Destination "./freertos_tcp/src_driver/linux" -Recurse

    New-Item    -Path "./freertos_tcp/src_driver/stm32h7_hal/"                           -ItemType Directory -ErrorAction SilentlyContinue
    Remove-Item -Path "./freertos_tcp/src_driver/stm32h7_hal/NetworkInterface.c"         -ErrorAction SilentlyContinue
    Copy-Item   -Path "./~temp/FreeRTOS-Plus-TCP-main/source/portable/NetworkInterface/STM32/NetworkInterface.c" -Destination "./freertos_tcp/src_driver/stm32h7_hal/NetworkInterface.c" -Recurse

    Remove-Item -Path "./freertos_tcp/include_driver/"                            -Recurse -ErrorAction SilentlyContinue
    New-Item    -Path "./freertos_tcp/include_driver/"                           -ItemType Directory
    Copy-Item   -Path "./~temp/FreeRTOS-Plus-TCP-main/source/portable/NetworkInterface/include/phyHandling.h" -Destination "./freertos_tcp/include_driver/phyHandling.h" -Recurse

    Remove-Item -Path "./freertos_tcp/src_driver/common"                            -Recurse -ErrorAction SilentlyContinue
    New-Item    -Path "./freertos_tcp/src_driver/common/"                           -ItemType Directory
    Copy-Item   -Path "./~temp/FreeRTOS-Plus-TCP-main/source/portable/NetworkInterface/Common/phyHandling.c" -Destination "./freertos_tcp/src_driver/common/phyHandling.c" -Recurse
    
    Remove-Item -Path "./freertos_tcp/include_iperf/"  -Recurse -ErrorAction SilentlyContinue
    New-Item    -Path "./freertos_tcp/include_iperf/"  -ItemType Directory
    Remove-Item -Path "./freertos_tcp/src_iperf/"      -Recurse -ErrorAction SilentlyContinue
    New-Item    -Path "./freertos_tcp/src_iperf/"      -ItemType Directory
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/htibosch/freertos_plus_projects/master/plus/Common/Utilities/iperf_task.h"       -OutFile "./freertos_tcp/include_iperf/iperf_task.h"
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/htibosch/freertos_plus_projects/master/plus/Common/Utilities/iperf_task_v3_0g.c" -OutFile "./freertos_tcp/src_iperf/iperf_task.c"
}

function Get-FreeRTOS-FAT(){
  Invoke-WebRequest -Uri "https://codeload.github.com/FreeRTOS/Lab-Project-FreeRTOS-FAT/zip/refs/heads/main"      -OutFile "./~temp/freertos_fat.zip"
  Expand-Archive -Path "./~temp/freertos_fat.zip" -DestinationPath "./~temp/"

  Remove-Item -Path "./freertos_fat/src"                        -Recurse -ErrorAction SilentlyContinue
  New-Item    -Path "./freertos_fat/src/"                       -ItemType Directory
  Copy-Item   -Path "./~temp/Lab-Project-FreeRTOS-FAT-main/*.c" -Destination "./freertos_fat/src/" -Recurse

  Remove-Item -Path "./freertos_fat/include/"                            -Recurse -ErrorAction SilentlyContinue
  New-Item    -Path "./freertos_fat/include/"                           -ItemType Directory
  Copy-Item   -Path "./~temp/Lab-Project-FreeRTOS-FAT-main/include/*.h" -Destination "./freertos_fat/include/" -Recurse
}

#
# Pipeline
#

Push-Location $PSScriptRoot

Remove-Item -Path "./~temp/" -Force -Recurse -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path "./~temp/" -ErrorAction SilentlyContinue

Get-CMSIS-Core
Get-DFP-STM32L4
Get-DFP-STM32H7
Get-USB-STM-Device
Get-FreeRTOS-Kernel
Get-FreeRTOS-TCP
Get-FreeRTOS-FAT

Pop-Location

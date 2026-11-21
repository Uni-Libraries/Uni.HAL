cmake_minimum_required(VERSION 3.29)
project(vendor_verification LANGUAGES C ASM)

# An update may replace several libraries together. Resolve every dependency
# against that same candidate set, using installed sources for the other libraries.
function(add_vendor_component name)
    set(source "${CMAKE_CURRENT_LIST_DIR}/../${name}")
    if(IS_DIRECTORY "${STAGED_VENDOR_DIR}/${name}")
        set(source "${STAGED_VENDOR_DIR}/${name}")
    endif()
    add_subdirectory("${source}" "${name}")
endfunction()

add_compile_definitions("UNI_HAL_TARGET_MCU_${UNI_HAL_TARGET_MCU}")
if(UNI_HAL_TARGET_MCU MATCHES "^STM32")
    set(UNI_HAL_HSE_VALUE 8000000)
    add_vendor_component(arm_cmsis_core)
    if(UNI_HAL_TARGET_MCU STREQUAL "STM32L496")
        add_vendor_component(st_stm32l4)
    else()
        add_vendor_component(st_stm32h7)
    endif()
endif()

if(IS_DIRECTORY "${STAGED_VENDOR_DIR}/freertos_kernel" OR
   (UNI_HAL_TARGET_MCU STREQUAL "STM32L496" AND IS_DIRECTORY "${STAGED_VENDOR_DIR}/freertos_fat"))
    add_vendor_component(freertos_kernel)
    if(UNI_HAL_TARGET_MCU STREQUAL "STM32L496" AND IS_DIRECTORY "${STAGED_VENDOR_DIR}/freertos_fat")
        add_vendor_component(freertos_fat)
    endif()
    if(NOT UNI_HAL_TARGET_MCU STREQUAL "PC")
        # The Arm ports and libc hooks are INTERFACE sources on freertos_kernel.
        add_library(kernel_port STATIC)
        target_compile_features(kernel_port PRIVATE c_std_23)
        target_link_libraries(kernel_port PRIVATE freertos_kernel)
    endif()
endif()

if(TARGET st_stm32h7 AND IS_DIRECTORY "${STAGED_VENDOR_DIR}/st_usb_device")
    add_vendor_component(st_usb_device)
    # The middleware expects the board to provide usbd_conf.h. Use the
    # configuration shipped with the candidate and select its STM32 header.
    configure_file("${STAGED_VENDOR_DIR}/st_usb_device/include_core/usbd_conf_template.h" usbd_conf.h COPYONLY)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/stm32fxxx.h"
        "#include <stm32h7xx_hal.h>\n#include <stm32h7xx_hal_pcd.h>\n")
    add_library(usb_device_stack STATIC)
    target_compile_definitions(usb_device_stack PRIVATE HAL_PCD_MODULE_ENABLED)
    target_include_directories(usb_device_stack PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(usb_device_stack PRIVATE st_usb_device)
endif()

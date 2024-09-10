#
# Library
#
add_library(st_stm32h7 STATIC)



#
# Sources
#
target_sources(st_stm32h7 PRIVATE
    "src_hal/stm32h7xx_hal.c"
    "src_hal/stm32h7xx_hal_cortex.c"
    "src_hal/stm32h7xx_hal_gpio.c"
    "src_hal/stm32h7xx_hal_eth.c"
    "src_hal/stm32h7xx_hal_eth_ex.c"
    "src_hal/stm32h7xx_hal_i2c.c"
    "src_hal/stm32h7xx_hal_i2c_ex.c"
    "src_hal/stm32h7xx_hal_rcc.c"
    "src_hal/stm32h7xx_hal_rcc_ex.c"

    src_hal/stm32h7xx_ll_adc.c
    src_hal/stm32h7xx_ll_dma.c
    src_hal/stm32h7xx_ll_gpio.c
    src_hal/stm32h7xx_ll_pwr.c
    src_hal/stm32h7xx_ll_rcc.c
    src_hal/stm32h7xx_ll_spi.c
    src_hal/stm32h7xx_ll_tim.c
    src_hal/stm32h7xx_ll_usart.c
    src_hal/stm32h7xx_ll_utils.c

    "src_device/system_stm32h7xx.c"

    "src_extra/systick.c"
)

target_sources(st_stm32h7 INTERFACE
    "src_device/startup_stm32h743xx.s"
)



#
# Includes
#
target_include_directories(st_stm32h7 PUBLIC "include_device")
target_include_directories(st_stm32h7 PUBLIC "include_hal")
target_include_directories(st_stm32h7 PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")



#
# Defines
#
target_compile_definitions(st_stm32h7 PRIVATE "DATA_IN_D2_SRAM")
target_compile_definitions(st_stm32h7 PUBLIC "USE_FULL_LL_DRIVER")
target_compile_definitions(st_stm32h7 PUBLIC "HSE_VALUE=${UNI_HAL_HSE_VALUE}")
if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    target_compile_definitions(st_stm32h7 PUBLIC "STM32H7")
    target_compile_definitions(st_stm32h7 PUBLIC "STM32H743xx")
else()
    message(FATAL_ERROR "Unknown MCU")
endif()




#
# Libraries
#
target_link_libraries(st_stm32h7 PUBLIC arm_cmsis_core)



#
# Config file
#
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/stm32h7xx_hal_conf.h"
    "#pragma once\n"
)
if(UNI_HAL_TARGET_MCU STREQUAL "STM32H743")
    file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/stm32h7xx_hal_conf.h"
        "#include \"stm32h7xx.h\"                             \n"
        "#define HAL_CORTEX_MODULE_ENABLED      (1)           \n"
        "#define HAL_DMA_MODULE_ENABLED         (1)           \n"
        "#define HAL_GPIO_MODULE_ENABLED        (1)           \n"
        "#define HAL_I2C_MODULE_ENABLED         (1)           \n"
        "#define HAL_RCC_MODULE_ENABLED         (1)           \n"
        "#define HAL_ETH_MODULE_ENABLED         (1)           \n"
        "#define USE_HAL_ETH_REGISTER_CALLBACKS (0)           \n"
        "                                                     \n"
        "#define ETH_RX_DESC_CNT                (16)          \n"
        "#define ETH_TX_DESC_CNT                (16)          \n"
        "                                                     \n"
        "#define HSE_STARTUP_TIMEOUT         (100UL)          \n"
        "#define LSE_STARTUP_TIMEOUT         (5000UL)         \n"
        "#define TICK_INT_PRIORITY           (0x0FUL)         \n"
        "                                                     \n"
        "#include \"stm32h7xx_ll_rcc.h\"                      \n"
        "                                                     \n"
        "#define assert_param(expr) ((void)0U)                \n"
        "#include \"stm32h7xx_hal_cortex.h\"                  \n"
        "#include \"stm32h7xx_hal_dma.h\"                     \n"
        "#include \"stm32h7xx_hal_flash.h\"                   \n"
        "#include \"stm32h7xx_hal_rcc.h\"                     \n"
        "#include \"stm32h7xx_hal_gpio.h\"                    \n"
        "#include \"stm32h7xx_hal_eth.h\"                     \n"
        "#include \"stm32h7xx_hal_eth_ex.h\"                  \n"
        "#include \"stm32h7xx_hal_i2c.h\"                     \n"
        "                                                     \n"
        "                                                     \n"
    )
endif()


#
# Linker
#

set_property(GLOBAL PROPERTY UNI_HAL_LINKER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src_linker/stm32_h743_zit.ld")

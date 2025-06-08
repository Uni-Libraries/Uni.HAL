#
# Library
#

add_library(uni.hal STATIC)



#
# Sources
#

file(GLOB_RECURSE src_interface CONFIGURE_DEPENDS
    "*_i.c"
)
file(GLOB_RECURSE src_common CONFIGURE_DEPENDS
    "*_c.c"
)
file(GLOB_RECURSE src_dfp CONFIGURE_DEPENDS
    ${UNI_HAL_DFP_SUFFIX}
)

target_sources(uni.hal PRIVATE ${src_common} ${src_dfp})
target_sources(uni.hal INTERFACE ${src_interface})



#
# Properties
#

#language standard)
if(MSVC)
    target_compile_features(uni.hal PUBLIC c_std_17)
else()
    target_compile_features(uni.hal PUBLIC c_std_23)
endif()


#
# Includes
#

target_include_directories(uni.hal PUBLIC ".")



#
# Definitions
#

target_compile_definitions(uni.hal PUBLIC UNI_HAL_TARGET_MCU_${UNI_HAL_TARGET_MCU})
if(UNI_HAL_CAN_USE_FREERTOS)
    target_compile_definitions(uni.hal PUBLIC UNI_HAL_CAN_USE_FREERTOS)
endif()
if(UNI_HAL_I2C_USE_FREERTOS)
    target_compile_definitions(uni.hal PUBLIC UNI_HAL_I2C_USE_FREERTOS)
endif()



#
# Libraries
#

target_link_libraries(uni.hal PUBLIC freertos_kernel)
target_link_libraries(uni.hal PUBLIC freertos_tcp)
target_link_libraries(uni.hal PUBLIC uni.common)
target_link_libraries(uni.hal PUBLIC nanoprintf)
if(DEFINED UNI_HAL_DFP_LIBRARY)
    target_link_libraries(uni.hal PUBLIC ${UNI_HAL_DFP_LIBRARY})
endif()
if(NOT UNI_HAL_TARGET_MCU STREQUAL "PC")
    target_link_libraries(uni.hal PUBLIC segger_rtt)
endif()

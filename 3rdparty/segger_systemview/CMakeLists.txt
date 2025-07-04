#
# Library
#

add_library(segger_systemview)

target_sources(segger_systemview PRIVATE
    src/SEGGER_SYSVIEW.c
)

target_include_directories(segger_systemview PUBLIC include)
target_include_directories(segger_systemview PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/segger_systemview_config/")

target_link_libraries(segger_systemview PUBLIC segger_rtt)

#language standard)
if(MSVC)
    target_compile_features(segger_systemview PRIVATE c_std_17)
else()
    target_compile_features(segger_systemview PRIVATE c_std_23)
endif()


file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/segger_systemview_config/SEGGER_SYSVIEW_Conf.h"
        "#ifndef SEGGER_SYSVIEW_CONF_H                                                  \n"
        "#define SEGGER_SYSVIEW_CONF_H                                                  \n"
        "                                                                               \n"
        "#define SEGGER_SYSVIEW_RTT_BUFFER_SIZE      (8192U)                            \n"
        "#define SEGGER_SYSVIEW_CPU_CACHE_LINE_SIZE  (32U  )                            \n"
        "#endif                                                                         \n"
        "                                                                               \n"
)


#
# FreeRTOS
#

add_library(segger_systemview_freertos INTERFACE)
target_sources(segger_systemview_freertos INTERFACE src_freertos/SEGGER_SYSVIEW_FreeRTOS.c)
target_include_directories(segger_systemview_freertos INTERFACE include_freertos)

# TODO: make configurable
#target_link_libraries(segger_systemview_freertos INTERFACE segger_systemview)

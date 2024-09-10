#
# Library
#

add_library(segger_rtt STATIC)



#
# Sources
#
target_sources(segger_rtt PRIVATE
    "src/SEGGER_RTT.c"
    "src/SEGGER_RTT_ASM_ARMv7M.S"
    "src/SEGGER_RTT_printf.c"
)



#
# Includes
#

target_include_directories(segger_rtt PUBLIC "include")



#
# Properties
#

#language standard)
if(MSVC)
    target_compile_features(segger_rtt PRIVATE c_std_17)
else()
    target_compile_features(segger_rtt PRIVATE c_std_23)
endif()

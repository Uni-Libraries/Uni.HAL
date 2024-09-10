#
# Library
#

add_library(freertos_fat STATIC)



#
# Sources
#
target_sources(freertos_fat PRIVATE
        src/ff_crc.c
        #src/ff_dev_support.c
        src/ff_dir.c
        src/ff_error.c
        src/ff_fat.c
        src/ff_file.c
        src/ff_format.c
        src/ff_ioman.c
        src/ff_locking.c
        src/ff_memory.c
        src/ff_stdio.c
        src/ff_string.c
        src/ff_sys.c
        src/ff_time.c
)



#
# Includes
#

target_include_directories(freertos_fat PUBLIC "include")
target_include_directories(freertos_fat PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/freertos_fat_config/")



#
# Libraries
#

target_link_libraries(freertos_fat PUBLIC freertos_kernel)



#
# Properties
#

#language standard)
if(MSVC)
    target_compile_features(freertos_fat PRIVATE c_std_17)
else()
    target_compile_features(freertos_fat PRIVATE c_std_23)
endif()

target_compile_options(freertos_fat PRIVATE -Wno-unused-but-set-variable)



#
# Config
#

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/freertos_fat_config/FreeRTOSFATConfig.h"
        "#ifndef FREERTOS_FAT_CONFIG_H                                                   \n"
        "#define FREERTOS_FAT_CONFIG_H                                                   \n"
        "                                                                                \n"
        "#define ffconfigBYTE_ORDER             (pdFREERTOS_LITTLE_ENDIAN)               \n"
        "#define ffconfigCWD_THREAD_LOCAL_INDEX (0)                                      \n"
        "#define ffconfigDEV_SUPPORT            (0)                                      \n"
        "#define ffconfigWRITE_BOTH_FATS        (1)                                      \n"
        "                                                                                \n"
        "#endif                                                                          \n"
)